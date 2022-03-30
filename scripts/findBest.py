#!/usr/bin/env python3
""" Finds the fastest or most space efficient store for each usage pattern. """

from pathlib import Path
import sys
import csv




if __name__ == "__main__":
    benchmark = Path(sys.argv[1])

    with open(benchmark, newline='') as csvfile:
        # hardware,store,op,size,records,data type,measurements,sum,min,max,avg
        reader = csv.DictReader(csvfile)
        rows = list(reader)

    groups = {}
    for row in rows:
        key = (row["hardware"], row["data type"], row["op"], row["size"], row["records"])
        if key not in groups:
            groups[key] = []
        for field in ["sum", "min", "max", "avg"]:
            row[field] = int(row[field])
        groups[key].append(row)

    results = {}
    threshold = 0.5
    for key, measurements in groups.items():
        (hardware, dataType, op, size, records) = key

        bestFunc = max if op == "space" else min

        best = bestFunc(measurements, key = lambda m: m["avg"])["avg"]
        bestList = measurements
        bestList = [m for m in measurements if (1 - threshold) * best < m["avg"] < (1 + threshold) * best]
        bestList = sorted(bestList, key = lambda m: m["avg"], reverse = (bestFunc == max))

        results[key] = bestList

    (lastHardware, lastDataType, lastOp, lastSize, lastRecords) = [None, None, None, None, None]
    for pattern, bests in sorted(results.items()):
        (hardware, dataType, op, size, records) = pattern
        if (hardware != lastHardware): print(f"{hardware}")
        if (dataType != lastDataType): print(f"    {dataType}")
        if (op != lastOp):             print(f"        {op}")
        if (size != lastSize):         print(f"            {size}")

        print(f"                {records} : " +  " / ".join(f"{b['store']} ({b['avg']})" for b in bests))

        (lastHardware, lastDataType, lastOp, lastSize, lastRecords) = pattern
