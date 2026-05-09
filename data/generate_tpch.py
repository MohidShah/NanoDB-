#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_tpch.py
Generates synthetic TPC-H-style pipe-delimited .tbl files:
    customer.tbl  - 20,000 rows
    orders.tbl    - 30,000 rows
    lineitem.tbl  - 50,000 rows
Run:  python data/generate_tpch.py
"""

import random
import os

random.seed(42)

DATA_DIR = os.path.dirname(os.path.abspath(__file__))

NATIONS = [
    "ALGERIA","ARGENTINA","BRAZIL","CANADA","EGYPT",
    "ETHIOPIA","FRANCE","GERMANY","INDIA","INDONESIA",
    "IRAN","IRAQ","JAPAN","JORDAN","KENYA",
    "MOROCCO","MOZAMBIQUE","PERU","CHINA","ROMANIA",
    "SAUDI ARABIA","VIETNAM","RUSSIA","UNITED KINGDOM","UNITED STATES"
]
SEGMENTS = ["AUTOMOBILE","BUILDING","FURNITURE","HOUSEHOLD","MACHINERY"]
PRIORITIES = ["1-URGENT","2-HIGH","3-MEDIUM","4-NOT SPECIFIED","5-LOW"]
SHIP_MODES = ["AIR","FOB","MAIL","RAIL","REG AIR","SHIP","TRUCK"]
SHIP_INSTRUCT = ["COLLECT COD","DELIVER IN PERSON","NONE","TAKE BACK RETURN"]
RETURN_FLAGS = ["A","N","R"]
LINE_STATUS  = ["F","O"]
CLERKS  = ["Clerk#%09d" % i for i in range(1, 51)]
WORDS   = ["special","pending","regular","express","furious","blithely","quickly","carefully"]

def rand_str(n=2):
    return " ".join(random.choices(WORDS, k=n))

def rand_phone():
    return "%d-%d-%d-%d" % (
        random.randint(10,99), random.randint(100,999),
        random.randint(100,999), random.randint(1000,9999))

def rand_date(start_year=1992, end_year=1998):
    y = random.randint(start_year, end_year)
    m = random.randint(1, 12)
    d = random.randint(1, 28)
    return "%d-%02d-%02d" % (y, m, d)

def gen_customer(n=20000):
    path = os.path.join(DATA_DIR, "customer.tbl")
    with open(path, "w") as f:
        for i in range(1, n+1):
            f.write("%d|Customer#%09d|%s|%d|%s|%.2f|%s|%s|\n" % (
                i, i, rand_str(3), random.randint(0,24),
                rand_phone(), round(random.uniform(-999.99, 9999.99), 2),
                random.choice(SEGMENTS), rand_str(4)))
    print("[+] customer.tbl: %d rows -> %s" % (n, path))

def gen_orders(n=30000, n_customers=20000):
    path = os.path.join(DATA_DIR, "orders.tbl")
    with open(path, "w") as f:
        for i in range(1, n+1):
            f.write("%d|%d|%s|%.2f|%s|%s|%s|%d|%s|\n" % (
                i, random.randint(1, n_customers),
                random.choice(["F","O","P"]),
                round(random.uniform(1000.0, 500000.0), 2),
                rand_date(), random.choice(PRIORITIES),
                random.choice(CLERKS), 0, rand_str(3)))
    print("[+] orders.tbl: %d rows -> %s" % (n, path))

def gen_lineitem(n=50000, n_orders=30000):
    path = os.path.join(DATA_DIR, "lineitem.tbl")
    with open(path, "w") as f:
        linenumber = 1
        prev_order = -1
        for i in range(1, n+1):
            lo = random.randint(1, n_orders)
            if lo == prev_order:
                linenumber += 1
            else:
                linenumber = 1
            prev_order = lo
            f.write("%d|%d|%d|%d|%.2f|%.2f|%.2f|%.2f|%s|%s|%s|%s|%s|%s|%s|%s|\n" % (
                lo, random.randint(1,200000), random.randint(1,10000), linenumber,
                round(random.uniform(1.0,50.0),2),
                round(random.uniform(1000.0,100000.0),2),
                round(random.uniform(0.0,0.10),2),
                round(random.uniform(0.0,0.08),2),
                random.choice(RETURN_FLAGS), random.choice(LINE_STATUS),
                rand_date(), rand_date(), rand_date(),
                random.choice(SHIP_INSTRUCT), random.choice(SHIP_MODES),
                rand_str(2)))
    print("[+] lineitem.tbl: %d rows -> %s" % (n, path))

if __name__ == "__main__":
    os.makedirs(DATA_DIR, exist_ok=True)
    gen_customer()
    gen_orders()
    gen_lineitem()
    print("[+] Done. Total rows: 100,000")
