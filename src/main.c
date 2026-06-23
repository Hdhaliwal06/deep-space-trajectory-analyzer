#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_RECORDS 64
#define AU_KM 149597870.7
#define LIGHT_SPEED_KM_S 299792.458

typedef struct {
    char date[20];
    int day_number;
    int estimated;
    double x_km;
    double y_km;
    double z_km;
    double vx_km_s;
    double vy_km_s;
    double vz_km_s;
} EphemerisRecord;

typedef struct {
    EphemerisRecord records[MAX_RECORDS];
    int count;
} MissionData;

double vector_length(double a, double b, double c) {
    return sqrt((a * a) + (b * b) + (c * c));
}

char *trim_whitespace(char *text) {
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return text;
}

int month_from_name(const char *month) {
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int i;

    for (i = 0; i < 12; i++) {
        if (strcmp(month, months[i]) == 0) {
            return i + 1;
        }
    }

    return 0;
}

int days_from_civil(int year, int month, int day) {
    int adjusted_year = year - (month <= 2);
    int era = (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
    unsigned yoe = (unsigned)(adjusted_year - era * 400);
    unsigned doy = (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)day - 1U;
    unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;

    return era * 146097 + (int)doe - 719468;
}

int parse_date_string(const char *date_text, int *day_number) {
    int year;
    int day;
    char month_text[4];
    int month;

    if (sscanf(date_text, "%d-%3[^-]-%d", &year, month_text, &day) != 3) {
        return 0;
    }

    month = month_from_name(month_text);
    if (month == 0) {
        return 0;
    }

    *day_number = days_from_civil(year, month, day);
    return 1;
}

const char *data_file_for_spacecraft(const char *spacecraft) {
    if (strcmp(spacecraft, "voyager1") == 0) {
        return "data/voyager1.txt";
    }

    if (strcmp(spacecraft, "voyager2") == 0) {
        return "data/voyager2.txt";
    }

    return NULL;
}

int parse_record(char line[], EphemerisRecord *record) {
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
    if (!parse_date_string(record->date, &record->day_number)) {
        return 0;
    }

    return 1;
}

int load_mission_data(const char *filename, MissionData *data) {
    FILE *file = fopen(filename, "r");
    char line[MAX_LINE];
    int inside_data_section = 0;

    if (file == NULL) {
        printf("Could not open data file: %s\n", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "$$SOE", 5) == 0) {
            inside_data_section = 1;
            continue;
        }

        if (strncmp(line, "$$EOE", 5) == 0) {
            break;
        }

        if (inside_data_section && data->count < MAX_RECORDS) {
            EphemerisRecord record;

            if (parse_record(line, &record)) {
                data->records[data->count++] = record;
            }
        }
    }

    fclose(file);
    return data->count > 0;
}

int interpolate_record(const MissionData *data, int target_day_number, EphemerisRecord *result) {
    int i;

    for (i = 0; i < data->count; i++) {
        if (data->records[i].day_number == target_day_number) {
            *result = data->records[i];
            result->estimated = 0;
            return 1;
        }
    }

    for (i = 0; i < data->count - 1; i++) {
        const EphemerisRecord *a = &data->records[i];
        const EphemerisRecord *b = &data->records[i + 1];
        double span = (double)(b->day_number - a->day_number);
        double fraction;

        if (target_day_number > a->day_number && target_day_number < b->day_number) {
            fraction = (double)(target_day_number - a->day_number) / span;

            snprintf(result->date, sizeof(result->date), "interpolated");
            result->day_number = target_day_number;
            result->estimated = 0;
            result->x_km = a->x_km + fraction * (b->x_km - a->x_km);
            result->y_km = a->y_km + fraction * (b->y_km - a->y_km);
            result->z_km = a->z_km + fraction * (b->z_km - a->z_km);
            result->vx_km_s = a->vx_km_s + fraction * (b->vx_km_s - a->vx_km_s);
            result->vy_km_s = a->vy_km_s + fraction * (b->vy_km_s - a->vy_km_s);
            result->vz_km_s = a->vz_km_s + fraction * (b->vz_km_s - a->vz_km_s);
            return 1;
        }
    }

    if (target_day_number < data->records[0].day_number) {
        const EphemerisRecord *a = &data->records[0];
        double delta_days = (double)(target_day_number - a->day_number);
        double seconds = delta_days * 86400.0;

        *result = *a;
        snprintf(result->date, sizeof(result->date), "estimated");
        result->day_number = target_day_number;
        result->estimated = 1;
        result->x_km = a->x_km + a->vx_km_s * seconds;
        result->y_km = a->y_km + a->vy_km_s * seconds;
        result->z_km = a->z_km + a->vz_km_s * seconds;
        return 1;
    }

    if (target_day_number > data->records[data->count - 1].day_number) {
        const EphemerisRecord *a = &data->records[data->count - 1];
        double delta_days = (double)(target_day_number - a->day_number);
        double seconds = delta_days * 86400.0;

        *result = *a;
        snprintf(result->date, sizeof(result->date), "estimated");
        result->day_number = target_day_number;
        result->estimated = 1;
        result->x_km = a->x_km + a->vx_km_s * seconds;
        result->y_km = a->y_km + a->vy_km_s * seconds;
        result->z_km = a->z_km + a->vz_km_s * seconds;
        return 1;
    }

    return 0;
}

void print_report(const char *spacecraft, EphemerisRecord record) {
    double distance_km = vector_length(record.x_km, record.y_km, record.z_km);
    double speed_km_s = vector_length(record.vx_km_s, record.vy_km_s, record.vz_km_s);
    double distance_au = distance_km / AU_KM;
    double light_delay_hours = (distance_km / LIGHT_SPEED_KM_S) / 3600.0;

    printf("\n");
    printf("DEEP SPACE TRAJECTORY ANALYZER\n");
    printf("----------------------------------------\n");
    printf("Spacecraft: %s\n", spacecraft);
    printf("Date:       %s\n", record.date);
    if (record.estimated) {
        printf("Mode:       estimated from nearest mission data point\n");
    } else {
        printf("Mode:       interpolated from mission data points\n");
    }
    printf("\n");
    printf("Position relative to Sun:\n");
    printf("  X: %.0f km\n", record.x_km);
    printf("  Y: %.0f km\n", record.y_km);
    printf("  Z: %.0f km\n", record.z_km);
    printf("\n");
    printf("Velocity relative to Sun:\n");
    printf("  VX: %.2f km/s\n", record.vx_km_s);
    printf("  VY: %.2f km/s\n", record.vy_km_s);
    printf("  VZ: %.2f km/s\n", record.vz_km_s);
    printf("\n");
    printf("Derived mission metrics:\n");
    printf("  Distance from Sun:       %.0f km\n", distance_km);
    printf("  Distance from Sun:       %.2f AU\n", distance_au);
    printf("  Speed relative to Sun:   %.2f km/s\n", speed_km_s);
    printf("  One-way light delay:     %.2f hours\n", light_delay_hours);
    printf("----------------------------------------\n");
}

int main(int argc, char *argv[]) {
    const char *filename;
    MissionData mission = {0};
    int target_day_number;
    EphemerisRecord record;

    if (argc != 3) {
        printf("Usage: %s <voyager1|voyager2> <date>\n", argv[0]);
        printf("Example: %s voyager1 2024-Jan-01\n", argv[0]);
        return 1;
    }

    filename = data_file_for_spacecraft(argv[1]);
    if (filename == NULL) {
        printf("Unknown spacecraft: %s\n", argv[1]);
        printf("Choose voyager1 or voyager2.\n");
        return 1;
    }

    if (!load_mission_data(filename, &mission)) {
        printf("No usable mission data found in %s.\n", filename);
        return 1;
    }

    if (!parse_date_string(argv[2], &target_day_number)) {
        printf("Invalid date format: %s\n", argv[2]);
        printf("Use the format YYYY-Mon-DD, like 2024-Jan-01.\n");
        return 1;
    }

    if (!interpolate_record(&mission, target_day_number, &record)) {
        printf("Date not found or error in data file.\n");
        return 1;
    }

    if (strcmp(record.date, "interpolated") == 0 || strcmp(record.date, "estimated") == 0) {
        snprintf(record.date, sizeof(record.date), "%s", argv[2]);
    }

    print_report(argv[1], record);
    return 0;
}
