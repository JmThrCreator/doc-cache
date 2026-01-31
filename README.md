# doc-cache

Scans a directory tree for PDFs and images, and generates a cache with:
- Thumnails for each document
- Full renders of PDF pages as images
- Nested folder support

Originally designed to come with a GUI, but reasoned myself out of impelenting a renderer in C 😅. Only supports MacOS, but written to be extensible.

Use: `doc-cache <input-dir>`
