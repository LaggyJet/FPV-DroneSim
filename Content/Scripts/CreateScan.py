import numpy as np
import csv
import os
import sys
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors

def lerp_color(t, c1, c2):
    return c1 * (1 - t) + c2 * t

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(script_dir, "scan_points.csv")
    if not os.path.exists(csv_path):
        print("scan_points.csv not found!")
        return
    points = []
    with open(csv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            points.append([float(row['X']), float(row['Y']), float(row['Z'])])
    points = np.array(points)
    points *= 0.0328084
    scan_dir = os.path.join(script_dir, "Scans")
    os.makedirs(scan_dir, exist_ok=True)
    ply_path = os.path.join(scan_dir, "scan_output.ply")
    with open(ply_path, 'w') as f:
        f.write("ply\nformat ascii 1.0\n")
        f.write(f"element vertex {len(points)}\n")
        f.write("property float x\nproperty float y\n")
        f.write("property float z\n")
        f.write("end_header\n")
        for p in points:
            f.write(f"{p[0]:.3f} {p[1]:.3f} {p[2]:.3f}\n")
    print(f"Saved scan_output.ply in {scan_dir} with {len(points)} points")

if __name__ == "__main__":
    main()
