import sys
import os
import numpy as np
import csv
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) < 2:
        print("Usage: CreateScan.py <scan_counter>")
        return
    try:
        counter = int(sys.argv[1])
    except ValueError:
        print("Invalid counter passed.")
        return
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
    ply_path = os.path.join(scan_dir, f"scan_output_{counter}.ply")
    with open(ply_path, 'w') as f:
        f.write("ply\nformat ascii 1.0\n")
        f.write(f"element vertex {len(points)}\n")
        f.write("property float x\nproperty float y\n")
        f.write("property float z\n")
        f.write("end_header\n")
        for p in points:
            f.write(f"{p[0]:.3f} {p[1]:.3f} {p[2]:.3f}\n")
    print(f"Saved scan_output_{counter}.ply in {scan_dir} with {len(points)} points")
    try:
        os.remove(csv_path)
        print(f"Deleted {csv_path}")
    except Exception as e:
        print(f"Failed to delete {csv_path}: {e}")

if __name__ == "__main__":
    main()