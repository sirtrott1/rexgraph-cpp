#!/usr/bin/env python3
"""Independent Fraction oracle over retained fixture declarations and outputs."""
from fractions import Fraction
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    output = subprocess.run([sys.argv[1], "--oracle"], text=True, capture_output=True, check=True).stdout
    cases = {}
    checks = 0
    case = None
    for line in output.splitlines():
        parts = line.split()
        tag, *p = parts
        if tag == "case":
            name = p[0]
            case = {"upper": {}, "lower": {}, "x": {}, "y": {}, "occ": {}, "m": {}}
            cases[name] = case
        elif tag in ("upper", "lower"):
            case[tag][int(p[0])] = int(p[1])
        elif tag in ("x", "y"):
            case[tag][int(p[0]), int(p[1])] = Fraction(p[2])
        elif tag == "occ":
            occurrence, upper, lower, active = map(int, p[:4])
            case["occ"][occurrence] = upper, lower, active, Fraction(p[4]), p[5]
        elif tag == "m":
            occurrence, row, col = map(int, p[:3])
            case["m"][occurrence, row, col] = Fraction(p[3])
        elif tag == "end":
            expected = {(key, i): Fraction(0) for key, n in case["lower"].items() for i in range(n)}
            require(set(case["y"]) == set(expected), f"{name}: missing lower support including zero")
            for occurrence, (upper, lower, active, coefficient, role) in case["occ"].items():
                if not active:
                    continue
                for row in range(case["lower"][lower]):
                    for col in range(case["upper"][upper]):
                        matrix = Fraction(row == col) if role == "constant" else case["m"].get((occurrence, row, col), Fraction(0))
                        realized = matrix if role == "realized" else coefficient * matrix
                        expected[lower, row] += realized * case["x"][upper, col]
            require(expected == case["y"], f"{name}: exact retained-occurrence action mismatch")
            checks += len(expected)
        elif tag != "checks":
            raise AssertionError(f"unknown oracle record: {tag}")
    require({"independent", "realized", "scalar", "zero", "zero_map", "wide", "empty", "both_changed", "field_changed", "map_changed", "permuted", "recoded"} <= cases.keys(), "missing required oracle cases")
    base = cases["independent"]
    require(cases["realized"]["y"] == base["y"], "realized and independent roles differ")
    require(cases["permuted"]["y"] == base["y"], "explicit coordinate permutation changed action")
    require(cases["recoded"]["y"] == base["y"], "source aspect migration changed action")
    require(any(v.numerator.bit_length() > 1000 for v in cases["wide"]["y"].values()), "wide exact growth missing")
    require(all(v == 0 for v in cases["zero"]["y"].values()), "supported zero changed")
    # Full finite defect plus innovation, with simultaneous map and field change.
    # This calculation retains T' eta and therefore its finite interaction term.
    for name in ("field_changed", "map_changed", "both_changed"):
        target = cases[name]
        predicted = {key: Fraction(0) for key in base["y"]}
        for occurrence, (upper, lower, active, coefficient, _) in base["occ"].items():
            if not active:
                continue
            for row in range(base["lower"][lower]):
                for col in range(base["upper"][upper]):
                    old_map = coefficient * base["m"].get((occurrence, row, col), Fraction(0))
                    new_map = coefficient * target["m"].get((occurrence, row, col), Fraction(0))
                    old_x = base["x"][upper, col]
                    eta = target["x"][upper, col] - old_x
                    predicted[lower, row] += (new_map - old_map) * old_x + new_map * eta
        require(predicted == {key: target["y"][key] - base["y"][key] for key in predicted}, f"{name}: finite defect/innovation identity failed")
        checks += len(predicted)
    print(f"Fraction oracle: {checks} output/finite-identity checks across {len(cases)} cases passed")


if __name__ == "__main__":
    main()
