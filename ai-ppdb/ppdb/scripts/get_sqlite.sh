#!/bin/bash

# SQLite version
VERSION="3450100"

# Create directories
mkdir -p ppdb/vendor/sqlite3
cd ppdb/vendor/sqlite3

# Download and extract SQLite
echo "Downloading SQLite library..."
curl -L -o sqlite.zip "https://www.sqlite.org/2024/sqlite-amalgamation-${VERSION}.zip"
unzip -o sqlite.zip
mv sqlite-amalgamation-${VERSION}/* .
rm -rf sqlite-amalgamation-${VERSION}
rm sqlite.zip

echo "SQLite library downloaded successfully" 
