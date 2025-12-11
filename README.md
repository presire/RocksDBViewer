# RocksDB GUI Viewer

A GUI tool for operating RocksDB databases from a browser. Access RocksDB via REST API and intuitively browse, edit, and delete data.

## Overview

This tool consists of the following components:

1. **Create_RocksDB.py** - Sample database creation script (for initial setup)
2. **RocksDB_API_Server.py** - REST API server for accessing RocksDB (Python/Flask)
3. **RocksDBViewer.html** - Web interface for data manipulation
4. **rocksdb-manager.js** - JavaScript code managing UI and API server integration

## Setup

### Requirements

- Python 3.7 or higher
- Modern web browser (Chrome, Firefox, Safari, etc.)

### Installing Python Packages

```bash
pip3 install flask flask-cors rocksdict
```

### File Structure

```
project/
├── Create_RocksDB.py          # Sample database creation script
├── RocksDB_API_Server.py      # API server
├── RocksDBViewer.html         # HTML interface
├── js/
│   └── rocksdb-manager.js    # JavaScript code
├── css/
│   ├── core.css              # Basic styles (optional)
│   └── tailwind.css          # Tailwind CSS styles (optional)
└── sample_rocksdb/            # RocksDB database (auto-generated)
```

**Note**  
CSS files are referenced in the HTML file, but since Tailwind utility classes are used, they can also be loaded via CDN.

## Usage

### 0. Creating a Sample Database (First Time Only)

If you want to try with sample data, you can create a sample database using the included `Create_RocksDB.py` script.

```bash
# Create sample database at default path (./sample_rocksdb)
python3 Create_RocksDB.py

# Create sample database at custom path
python3 Create_RocksDB.py /path/to/your/rocksdb
```

**Sample data created:**
- `default` - Application basic information and settings (7 records)
- `users` - User information (5 records)
- `products` - Product data (7 records)
- `orders` - Order history (5 records)
- `config` - System configuration (17 records)
- `logs` - Application logs (20 records)

After running the script, the following will be displayed:

```
Sample database creation completed!
Database path: /absolute/path/to/sample_rocksdb

Created column families:
   • default      :   7 records
   • users        :   5 records
   • products     :   7 records
   • orders       :   5 records
   • config       :  17 records
   • logs         :  20 records
```

**Note**  
If an existing database exists, an overwrite confirmation prompt will be displayed.

### 1. Starting the API Server

First, start the RocksDB API server.

```bash
# Using default path (./sample_rocksdb)
python3 RocksDB_API_Server.py

# Specifying a custom database path
python3 RocksDB_API_Server.py /path/to/your/rocksdb
```

When the server starts successfully, the following will be displayed:

```
============================================================
RocksDB REST API Server
============================================================

Database path: /absolute/path/to/sample_rocksdb
Server URL: http://localhost:5000

Available endpoints:
  GET    /api/health                     - Health check
  GET    /api/column-families            - Column family list
  GET    /api/data/<cf>                  - Get all data
  ...

To stop server: Ctrl+C
============================================================
```

### 2. Opening the HTML Viewer

1. Open `RocksDBViewer.html` in a web browser
2. Enter `http://localhost:5000` in the "API Server URL" field (preset by default)
3. Click the "Connect to Server" button

When the connection is successful, the status in the upper right of the screen changes to "Connected" and the main content is displayed.

### 3. Data Operations

#### Selecting a Column Family

Select the column family you want to work with from the dropdown menu at the top of the screen.

Available column families:
- `default` - Default column family
- `users` - User information
- `products` - Product information
- `orders` - Order information
- `config` - Configuration information
- `logs` - Log information

#### Displaying Data

All keys and values of the selected column family are displayed in table format.

#### Searching Data

Enter a keyword in the search box to filter data containing it in keys or values in real-time.

#### Adding/Editing Data

1. Click the "Add New" button (or click the "Edit" button for existing data)
2. Enter key and value in the modal dialog
3. Click the "Save" button

**JSON Editing Features:**
- "Format" button - Format JSON for easy viewing
- "Minify" button - Compress JSON to a single line
- Real-time JSON validation - Check syntax errors during input

#### Deleting Data

- **Individual deletion**: Click the "Delete" button for each data row
- **Delete all**: Delete all data in the currently selected column family with the "Clear All" button

#### Exporting Data

Click the "Export" button to download data from the currently selected column family in JSON format.

#### Importing Data

1. Click the "Import" button
2. Select a JSON format file
3. Data is automatically imported

**Import file format:**
```json
{
  "key1": "value1",
  "key2": {"name": "example", "age": 30},
  "key3": "value3"
}
```

## API Endpoints

The API server provides the following endpoints:

### Health Check
```
GET /api/health
```
Get server status and database information

### Column Family List
```
GET /api/column-families
```
Get list of available column families

### Get Data
```
GET /api/data/<column_family>
GET /api/data/<column_family>?search=keyword
GET /api/data/<column_family>/<key>
```
Get data (all, search, key-specific)

### Add/Update Data
```
POST /api/data/<column_family>
Content-Type: application/json

{
  "key": "my-key",
  "value": "my-value"
}
```

### Delete Data
```
DELETE /api/data/<column_family>/<key>
DELETE /api/data/<column_family>/clear
```
Individual deletion or delete all

### Export
```
GET /api/export/<column_family>
```
Export data in JSON format

### Import
```
POST /api/import/<column_family>
Content-Type: application/json

{
  "key1": "value1",
  "key2": "value2"
}
```

## Usage Examples

### Example 1: User Information Management

```json
// Key: user:1001
// Value:
{
  "id": 1001,
  "name": "Taro Yamada",
  "email": "taro@example.com",
  "role": "admin"
}
```

### Example 2: Configuration Storage

```json
// Key: config:app
// Value:
{
  "app_name": "MyApp",
  "version": "1.0.0",
  "debug_mode": false
}
```

### Example 3: Simple Cache Data

```
Key: cache:session:abc123
Value: {"user_id": 42, "expires": 1234567890}
```

## Feature List

### Main Features

- ✅ Real-time search and filtering
- ✅ JSON syntax validation and formatting
- ✅ Data import/export (JSON format)
- ✅ Multiple column family support
- ✅ Responsive design (mobile compatible)
- ✅ Copy to clipboard functionality
- ✅ Operation feedback via toast notifications
- ✅ Data addition, editing, and deletion
- ✅ Custom scrollbar
- ✅ Language toggle (Japanese/English)

### UI Features

- Smooth user experience with animation effects
- Visual feedback through color coding
- Automatic truncation display for long data
- Automatic detection and badge display for JSON data

## Security Notes

- This tool is intended for use in **development environments**
- If using in production, implement the following measures:
  - Authentication and authorization
  - Use HTTPS communication
  - Review CORS settings
  - Strengthen input validation
  - Implement API rate limiting

## Troubleshooting

### Cannot Connect to Server

**Cause 1**: API server is not running

```bash
# Start the server
python3 RocksDB_API_Server.py
```

**Cause 2**: Incorrect URL

- Default URL: `http://localhost:5000`
- Change port number appropriately if using a different port

**Cause 3**: CORS error

- Verify that CORS is enabled on the API server
- Check error messages in the browser console

### Database Not Found

```bash
# Create sample database
python3 Create_RocksDB.py ./Example

# Or create at default path
python3 Create_RocksDB.py
```

### Data Not Displaying

1. Check if the column family is correctly selected
2. Check if data exists in the database
3. Check error logs in the browser console

### Errors During JSON Editing

- Verify that the JSON format is correct
- Use double quotes (`"`) (single quotes not allowed)
- Be careful with trailing commas

## License

This project is freely available for educational and development purposes.

## Contributing

Bug reports and feature improvement suggestions are welcome.

## Support

If you encounter problems, check the following:
1. Python and packages are correctly installed
2. Database path is correct
3. TCP port 5000 is not being used by other programs
4. Error messages in the browser's JavaScript console

---

<br>

**Enjoy using RocksDB GUI Viewer!**
