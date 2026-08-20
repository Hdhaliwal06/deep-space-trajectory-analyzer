const form = document.querySelector("#query");
const status = document.querySelector("#status");
const results = document.querySelector("#results");
const fixed = (number, digits = 2) => Number(number).toLocaleString(undefined, {maximumFractionDigits: digits});
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const params = new URLSearchParams(new FormData(form));
  status.textContent = "Running the C analyzer…"; results.hidden = true;
  try {
    const response = await fetch("/api/analyze?" + params);
    const data = await response.json();
    if (!response.ok) throw new Error(data.error);
    document.querySelector("#mode").textContent = data.mode.toUpperCase() + " RESULT · " + data.requested_date;
    document.querySelector("#au").textContent = fixed(data.distance_au) + " AU";
    document.querySelector("#km").textContent = fixed(data.distance_km, 0) + " km";
    document.querySelector("#speed").textContent = fixed(data.speed_km_s);
    document.querySelector("#delay").textContent = fixed(data.light_delay_hours);
    document.querySelector("#vector").textContent = "Position  [km]    x " + fixed(data.position_km.x, 0) + "   y " + fixed(data.position_km.y, 0) + "   z " + fixed(data.position_km.z, 0) + "\nVelocity [km/s]  x " + fixed(data.velocity_km_s.x) + "   y " + fixed(data.velocity_km_s.y) + "   z " + fixed(data.velocity_km_s.z);
    document.querySelector("#source").textContent = "Cached source range: " + data.source_range.first + " to " + data.source_range.last + ".";
    status.textContent = data.mode === "estimated" ? "Estimated outside the cached range; treat as illustrative." : "Analysis complete.";
    results.hidden = false;
  } catch (error) { status.textContent = error.message; }
});
