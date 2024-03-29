#!/usr/bin/env python3
# Install dependencies:
#   python3 -m pip install gutenberg

import random
import os
import shutil
import sys
from pathlib import Path
from gutenberg.acquire import load_etext
from gutenberg.cleanup import strip_headers

def fetch_random_books(dest, count):
    dest = Path(dest).resolve()
    (dest / "cache").mkdir(exist_ok=True, parents=True)
    os.chdir(dest / "cache")

    downloaded_books = [int(p.stem) for p in dest.glob("*.txt")] # Get already downloaded books

    if len(downloaded_books) > count:  # Delete books if more than count
        while len(downloaded_books) > count:
            book_id = random.choice(downloaded_books)
            downloaded_books.remove(book_id)
            (dest / f"{book_id}.txt").unlink()
    else:
        while len(downloaded_books) < count:
            book_id = random.randint(1, 60000)  # Generate a random book ID
            if book_id in downloaded_books:
                continue

            try:
                text = strip_headers(load_etext(book_id)).strip() + "\n"
            except:
                continue # Some of them seem to just fail, so download another.

            downloaded_books.append(book_id)
            (dest / f"{book_id}.txt").write_text(text)

if __name__ == "__main__":
    count = int(sys.argv[1] if len(sys.argv) >= 2 else 250)

    dest = Path(__file__).parents[1].resolve() / "randomText"
    fetch_random_books(dest, count)