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
        for field in ["records", "sum", "min", "max", "avg"]:
            row[field] = int(row[field])
        key = (row["hardware"], row["data type"], row["op"], row["size"], row["records"])
        if key not in groups:
            groups[key] = []
        groups[key].append(row)

    # {hardware: {op: {size: {dataType: {records: [best...]}}}}}
    matrix = {}
    threshold = 0.05
    for (hardware, dataType, op, size, records), measurements in groups.items():
        rowKey = (hardware, op, size, dataType)

        bestFunc = max if op == "space" else min
        best = bestFunc(measurements, key = lambda m: m["avg"])["avg"]
        bestList = measurements
        bestList = [m for m in measurements if (1 - threshold) * best < m["avg"] < (1 + threshold) * best]
        bestList = sorted(bestList, key = lambda m: m["avg"], reverse = (bestFunc == max))

        if rowKey not in matrix: matrix[rowKey] = {r: [] for r in [100, 1_000, 10_000, 100_000, 1_000_000]}
        matrix[rowKey][records] = bestList

    def sortFunc(key):
        opOrder = ["insert", "update", "get", "remove", "space", "memory"]
        sizeOrder = ["1B to 1KiB", "1KiB to 10KiB", "10KiB to 100KiB", "100KiB to 1MiB"]
        dataTypeOrder = ["incompressible", "compressible"]

        return (key[0], opOrder.index(key[1]), sizeOrder.index(key[2]), dataTypeOrder.index(key[3]))

    def valToStr(op, val):
        if op == "space": return f"{val}%"
        elif op == "memory": return f"{int(round(val / 1024, 0))} MiB"
        else: return f"{int(round(val / 1000, 0))} μs"
    
    output = ""
    for (hardware, op, size, dataType), row in sorted(matrix.items(), key = lambda k: sortFunc(k[0])):
        output += f"{hardware},{op},{size},{dataType}"
        for records, bests in sorted(row.items()):
            output += "," + " / ".join(f"{b['store']} ({valToStr(op, b['avg'])})" for b in bests)
        output += "\n"

    print(output, end = "")
