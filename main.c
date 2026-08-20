#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define INITIAL_RECORD_CAPACITY 128
#define AU_KM 149597870.7
#define LIGHT_SPEED_KM_S 299792.458

typedef enum {
    MODE_EXACT,
    MODE_INTERPOLATED,
    MODE_ESTIMATED
} ResultMode;

typedef struct {
    char date[20];
    int day_number;
    double x_km;
    double y_km;
    double z_km;
    double vx_km_s;
    double vy_km_s;
    double vz_km_s;
} EphemerisRecord;

typedef struct {
    EphemerisRecord *records;
    size_t count;
    size_t capacity;
} MissionData;

static double vector_length(double a, double b, double c) {
    return sqrt(a * a + b * b + c * c);
}

static char *trim_whitespace(char *text) {
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
        text++;
    }
    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end-- = '\0';
    }
    return text;
}

static int month_from_name(const char *month) {
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int i;

    for (i = 0; i < 12; ++i) {
        if (strcmp(month, months[i]) == 0) {
            return i + 1;
        }
    }
    return 0;
}

static int days_in_month(int year, int month) {
    static const int normal[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month != 2) {
        return normal[month];
    }
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
}

static int days_from_civil(int year, int month, int day) {
    int y = year - (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U
                   + (unsigned)day - 1U;

    return era * 146097 + (int)(yoe * 365U + yoe / 4U - yoe / 100U + doy) - 719468;
}

static int parse_date_string(const char *text, int *day_number) {
    int year;
    int day;
    int month;
    char month_text[4] = {0};

    if (sscanf(text, "%d-%3[^-]-%d", &year, month_text, &day) != 3) {
        return 0;
    }
    month = month_from_name(month_text);
    if (year < 1600 || month == 0 || day < 1 || day > days_in_month(year, month)) {
        return 0;
    }

    *day_number = days_from_civil(year, month, day);
    return 1;
}

static const char *data_file_for_spacecraft(const char *spacecraft) {
    if (strcmp(spacecraft, "voyager1") == 0) {
        return "data/Voyager1.txt";
    }
    if (strcmp(spacecraft, "voyager2") == 0) {
        return "data/Voyager2.txt";
    }
    return NULL;
}

static int init_mission_data(MissionData *data) {
    data->records = malloc(INITIAL_RECORD_CAPACITY * sizeof(*data->records));
    if (!data->records) {
        return 0;
    }

    data->count = 0;
    data->capacity = INITIAL_RECORD_CAPACITY;
    return 1;
}

static void free_mission_data(MissionData *data) {
    free(data->records);
    data->records = NULL;
    data->count = 0;
    data->capacity = 0;
}

static int append_mission_record(MissionData *data, EphemerisRecord record) {
    if (data->count == data->capacity) {
        size_t capacity = data->capacity * 2;
        EphemerisRecord *expanded = realloc(data->records, capacity * sizeof(*data->records));

        if (!expanded) {
            return 0;
        }
        data->records = expanded;
        data->capacity = capacity;
    }

    data->records[data->count++] = record;
    return 1;
}

static int parse_record(char line[], EphemerisRecord *record) {
    char date_text[20];

    if (sscanf(line, " %19[^,],%lf,%lf,%lf,%lf,%lf,%lf",
               date_text,
               &record->x_km,
               &record->y_km,
               &record->z_km,
               &record->vx_km_s,
               &record->vy_km_s,
               &record->vz_km_s) != 7) {
        return 0;
    }

    snprintf(record->date, sizeof(record->date), "%s", trim_whitespace(date_text));
    return parse_date_string(record->date, &record->day_number);
}

static int load_mission_data(const char *filename, MissionData *data) {
    FILE *file = fopen(filename, "r");
    char line[MAX_LINE];
    int inside_data = 0;

    if (!file) {
        fprintf(stderr, "Could not open data file: %s\n", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        EphemerisRecord record;

        if (strncmp(line, "$$SOE", 5) == 0) {
            inside_data = 1;
            continue;
        }
        if (strncmp(line, "$$EOE", 5) == 0) {
            break;
        }
        if (!inside_data || !parse_record(line, &record)) {
            continue;
        }
        if (data->count && record.day_number <= data->records[data->count - 1].day_number) {
            fprintf(stderr, "Data file is not strictly date-sorted: %s\n", filename);
            fclose(file);
            return 0;
        }
        if (!append_mission_record(data, record)) {
            fprintf(stderr, "Could not allocate mission data.\n");
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    if (!data->count) {
        fprintf(stderr, "No usable ephemeris records in %s\n", filename);
    }
    return data->count > 0;
}

static void estimate_from(const EphemerisRecord *source, int target_day, EphemerisRecord *result) {
    double seconds = (double)(target_day - source->day_number) * 86400.0;

    *result = *source;
    result->day_number = target_day;
    result->x_km += result->vx_km_s * seconds;
    result->y_km += result->vy_km_s * seconds;
    result->z_km += result->vz_km_s * seconds;
}

static ResultMode resolve_record(const MissionData *data, int target_day, EphemerisRecord *result) {
    size_t low = 0;
    size_t high = data->count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;

        if (data->records[mid].day_number < target_day) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (low < data->count && data->records[low].day_number == target_day) {
        *result = data->records[low];
        return MODE_EXACT;
    }
    if (low == 0) {
        estimate_from(&data->records[0], target_day, result);
        return MODE_ESTIMATED;
    }
    if (low == data->count) {
        estimate_from(&data->records[data->count - 1], target_day, result);
        return MODE_ESTIMATED;
    }

    {
        const EphemerisRecord *a = &data->records[low - 1];
        const EphemerisRecord *b = &data->records[low];
        double f = (double)(target_day - a->day_number)
                   / (double)(b->day_number - a->day_number);

        *result = *a;
        result->day_number = target_day;
        result->x_km += f * (b->x_km - a->x_km);
        result->y_km += f * (b->y_km - a->y_km);
        result->z_km += f * (b->z_km - a->z_km);
        result->vx_km_s += f * (b->vx_km_s - a->vx_km_s);
        result->vy_km_s += f * (b->vy_km_s - a->vy_km_s);
        result->vz_km_s += f * (b->vz_km_s - a->vz_km_s);
        return MODE_INTERPOLATED;
    }
}

static const char *mode_name(ResultMode mode) {
    if (mode == MODE_EXACT) {
        return "exact";
    }
    if (mode == MODE_INTERPOLATED) {
        return "interpolated";
    }
    return "estimated";
}

static void print_json(const char *spacecraft,
                       const char *date,
                       const EphemerisRecord *record,
                       ResultMode mode,
                       const MissionData *data) {
    double distance = vector_length(record->x_km, record->y_km, record->z_km);
    double speed = vector_length(record->vx_km_s, record->vy_km_s, record->vz_km_s);

    printf("{\"spacecraft\":\"%s\",\"requested_date\":\"%s\",\"mode\":\"%s\",",
           spacecraft, date, mode_name(mode));
    printf("\"source_range\":{\"first\":\"%s\",\"last\":\"%s\"},",
           data->records[0].date, data->records[data->count - 1].date);
    printf("\"position_km\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},",
           record->x_km, record->y_km, record->z_km);
    printf("\"velocity_km_s\":{\"x\":%.9f,\"y\":%.9f,\"z\":%.9f},",
           record->vx_km_s, record->vy_km_s, record->vz_km_s);
    printf("\"distance_km\":%.6f,\"distance_au\":%.6f,\"speed_km_s\":%.6f,"
           "\"light_delay_hours\":%.6f}\n",
           distance,
           distance / AU_KM,
           speed,
           distance / LIGHT_SPEED_KM_S / 3600.0);
}

static void print_report(const char *spacecraft,
                         const char *date,
                         const EphemerisRecord *record,
                         ResultMode mode) {
    double distance = vector_length(record->x_km, record->y_km, record->z_km);
    double speed = vector_length(record->vx_km_s, record->vy_km_s, record->vz_km_s);

    printf("\nDEEP SPACE TRAJECTORY ANALYZER\n");
    printf("----------------------------------------\n");
    printf("Spacecraft: %s\n", spacecraft);
    printf("Date:       %s\n", date);
    printf("Mode:       %s\n\n", mode_name(mode));
    printf("Position relative to Sun (km):\n");
    printf("  X: %.0f\n", record->x_km);
    printf("  Y: %.0f\n", record->y_km);
    printf("  Z: %.0f\n\n", record->z_km);
    printf("Velocity relative to Sun (km/s):\n");
    printf("  VX: %.2f\n", record->vx_km_s);
    printf("  VY: %.2f\n", record->vy_km_s);
    printf("  VZ: %.2f\n\n", record->vz_km_s);
    printf("Derived mission metrics:\n");
    printf("  Distance from Sun:     %.0f km (%.2f AU)\n", distance, distance / AU_KM);
    printf("  Speed:                 %.2f km/s\n", speed);
    printf("  One-way light delay:   %.2f hours\n",
           distance / LIGHT_SPEED_KM_S / 3600.0);
    if (mode == MODE_ESTIMATED) {
        printf("\nWarning: constant-velocity extrapolation is not precision orbit propagation.\n");
    }
    printf("----------------------------------------\n");
}

int main(int argc, char *argv[]) {
    const char *filename;
    MissionData mission;
    EphemerisRecord record;
    int target_day;
    int json;
    ResultMode mode;

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s <voyager1|voyager2> <YYYY-Mon-DD> [--json]\n", argv[0]);
        return 1;
    }
    if (argc == 4 && strcmp(argv[3], "--json") != 0) {
        fprintf(stderr, "Unknown option: %s\n", argv[3]);
        return 1;
    }

    filename = data_file_for_spacecraft(argv[1]);
    if (!filename) {
        fprintf(stderr, "Unknown spacecraft: %s. Choose voyager1 or voyager2.\n", argv[1]);
        return 1;
    }
    if (!parse_date_string(argv[2], &target_day)) {
        fprintf(stderr, "Invalid date: %s. Use YYYY-Mon-DD.\n", argv[2]);
        return 1;
    }
    if (!init_mission_data(&mission)) {
        fprintf(stderr, "Could not allocate mission data.\n");
        return 1;
    }
    if (!load_mission_data(filename, &mission)) {
        free_mission_data(&mission);
        return 1;
    }

    mode = resolve_record(&mission, target_day, &record);
    json = argc == 4;
    if (json) {
        print_json(argv[1], argv[2], &record, mode, &mission);
    } else {
        print_report(argv[1], argv[2], &record, mode);
    }

    free_mission_data(&mission);
    return 0;
}
