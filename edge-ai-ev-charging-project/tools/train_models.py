"""
train_models.py
================
Trains the two Edge AI models used by the Smart EV Charging Station
Optimizer and exports them as C++ (src/model.h), matching the format
already embedded in the firmware.

Models
------
1. Arrival probability  -> LogisticRegression   -> exported with m2cgen
2. Session duration      -> DecisionTreeRegressor -> exported with micromlgen

Install once:
    pip install scikit-learn pandas numpy m2cgen micromlgen

Run:
    python train_models.py
Output:
    ../src/model.h   (overwrite only after you've backed up / diffed it)

IMPORTANT: this uses a SYNTHETIC dataset (generate_dataset below) so the
project runs standalone. For your report, replace generate_dataset() with
real logged telemetry (export from ThingsBoard -> CSV) once you have a
few days of data, then re-run this script and re-flash. A model trained
on synthetic data is fine for a working demo; say so explicitly in your
report's limitations section — don't claim it's trained on real usage
data if it isn't.
"""

import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeRegressor
import m2cgen as m2c
from micromlgen import port

FEATURES = [
    "hourOfDay", "dayOfWeek", "bayOccupied",
    "recentAvgCurrent", "sessionElapsedMin", "historicalArrivalRate",
]

HOURLY_RATE = [
    0.02, 0.01, 0.01, 0.01, 0.02, 0.05,
    0.10, 0.22, 0.35, 0.28, 0.15, 0.10,
    0.08, 0.07, 0.07, 0.09, 0.14, 0.24,
    0.38, 0.42, 0.30, 0.18, 0.09, 0.04,
]


def generate_dataset(n=4000, seed=42):
    """Synthetic but structured EV-bay usage data: morning/evening arrival
    peaks, session length shrinking the longer a car has already charged."""
    rng = np.random.default_rng(seed)
    hour = rng.integers(0, 24, n)
    dow = rng.integers(0, 7, n)
    hist_rate = np.array([HOURLY_RATE[h] for h in hour])
    bay_occupied = rng.integers(0, 2, n)
    session_elapsed = np.where(bay_occupied == 1, rng.uniform(0, 240, n), 0)
    avg_current = np.where(bay_occupied == 1, rng.uniform(4, 32, n), 0)

    # Arrival label: more likely near historical peaks, noisy
    arrival_logit = (
        -2.5 + 4.8 * hist_rate + 0.08 * bay_occupied
        - 0.005 * session_elapsed - 0.03 * dow
    )
    arrival_prob = 1 / (1 + np.exp(-arrival_logit))
    arrival = rng.binomial(1, arrival_prob)

    # Duration label: sessions that start with higher current tend to be
    # shorter fast-charge top-ups; longer sessions decay in elapsed time.
    duration = np.clip(
        220 - 0.9 * session_elapsed - 1.5 * avg_current + rng.normal(0, 15, n),
        0, 240,
    )

    df = pd.DataFrame({
        "hourOfDay": hour,
        "dayOfWeek": dow,
        "bayOccupied": bay_occupied,
        "recentAvgCurrent": avg_current,
        "sessionElapsedMin": session_elapsed,
        "historicalArrivalRate": hist_rate,
        "arrival": arrival,
        "durationMin": duration,
    })
    return df


def main():
    df = generate_dataset()
    X = df[FEATURES].values

    # ---- Model 1: arrival probability ----
    y_arrival = df["arrival"].values
    logreg = LogisticRegression(max_iter=1000)
    logreg.fit(X, y_arrival)
    print("Arrival model train accuracy:", logreg.score(X, y_arrival))

    arrival_cpp = m2c.export_to_c(logreg)

    # ---- Model 2: session duration ----
    y_duration = df["durationMin"].values
    tree = DecisionTreeRegressor(max_depth=6, min_samples_leaf=20, random_state=0)
    tree.fit(X, y_duration)
    print("Duration model train R^2:", tree.score(X, y_duration))

    tree_cpp = port(tree)

    # ---- Stitch into model.h ----
    # m2cgen/micromlgen output varies by version; if the generated code
    # doesn't match the predictArrival/predictDuration wrapper signatures
    # already in src/model.h, keep the hand-written wrappers there and
    # just paste in the updated coefficient expression / tree body.
    print("\n--- m2cgen logistic regression output ---\n")
    print(arrival_cpp)
    print("\n--- micromlgen decision tree output ---\n")
    print(tree_cpp)
    print(
        "\nCopy the coefficients / tree branches above into "
        "../src/model.h, keeping the existing predictArrival()/"
        "predictDuration() wrapper functions so main.cpp doesn't change."
    )


if __name__ == "__main__":
    main()
