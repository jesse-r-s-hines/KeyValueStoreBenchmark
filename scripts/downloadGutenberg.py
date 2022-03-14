#!/usr/bin/env python3
# Install dependencies:
#   python3 -m pip gutenbergpy

import random, os, re, shutil, sys
from pathlib import Path
from gutenbergpy.gutenbergcache import GutenbergCache
from gutenbergpy import textget

def fetch_random_books(dest, count):
    dest = Path(dest).resolve()
    (dest / "cache").mkdir(exist_ok=True, parents=True)
    os.chdir(dest / "cache")

    if not GutenbergCache.exists():
        # This is a one-time thing per machine
        GutenbergCache.create()
    cache = GutenbergCache.get_cache()

    downloaded_books = [int(p.stem) for p in dest.glob("*.txt")] # Get already downloaded books
    all_books = cache.query(downloadtype = ['text/plain'], languages = ['en'])
    all_books = list(set(all_books) - set(downloaded_books))

    if len(downloaded_books) > count:  # Delete books if more than count
        while len(downloaded_books) > count:
            book_id = random.choice(downloaded_books)
            downloaded_books.remove(book_id)
            (dest / f"{book_id}.txt").unlink()
    else:
        while len(downloaded_books) < count:
            book_id = random.choice(all_books)
            all_books.remove(book_id)

            try:
                text = textget.get_text_by_id(book_id)
            except:
                continue # Some of them seem to just fail, so download another.
            text = textget.strip_headers(text).decode().strip() + "\n"

            downloaded_books.append(book_id)
            (dest / f"{book_id}.txt").write_text(text)

if __name__ == "__main__":
    count = int(sys.argv[1] if len(sys.argv) >= 2 else 250)

    dest = Path(__file__).parents[1].resolve() / "randomText"
    fetch_random_books(dest, count)