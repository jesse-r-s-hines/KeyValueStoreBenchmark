#!/usr/bin/env python3
# Install dependencies:
#   python3 -m pip gutenbergpy

import random, os, re, shutil
from pathlib import Path
from gutenbergpy.gutenbergcache import GutenbergCache
from gutenbergpy import textget

def retry(func, times):
    for attempt in range(times - 1):
        try:
            return func()
        except:
            pass
    return func() # let error fall through

def fetch_random_books(dest, count):
    dest = Path(dest).resolve()
    (dest / "cache").mkdir(exist_ok=True, parents=True)
    os.chdir(dest / "cache")

    if not GutenbergCache.exists():
        # This is a one-time thing per machine
        GutenbergCache.create()

    cache = GutenbergCache.get_cache()

    books = cache.query(downloadtype=['text/plain'])
    downloaded = []
    
    while len(downloaded) < count:
        book_id = random.choice(books)
        books.remove(book_id)
        try:
            text = textget.get_text_by_id(book_id)
        except:
            continue # Some of them seem to just fail?
        downloaded.append(book_id)

        name = cache.native_query(f"""
            SELECT name
            FROM titles t
                JOIN books b on t.bookid = b.id
            WHERE b.id = {int(book_id)}
        """).fetchone()[0]

        file_name = re.sub(r"'", "", name)
        file_name = file_name.splitlines()[0]
        file_name = re.sub(r"\W", " ", file_name).title().replace(" ", "")
        file_name = file_name[0:64]
        file_name = file_name + ".txt"

        (dest / file_name).write_text(text.decode())

if __name__ == "__main__":
    dest = Path(__file__).parent.parent.resolve() / "out/datasets/compressible"
    fetch_random_books(dest, 100)