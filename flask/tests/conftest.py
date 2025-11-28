"""
pytest configuration for Flask tests
Initializes database and common fixtures
"""

import pytest
import sys
import os

# Add parent directory to path so we can import flask modules
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

# Import and initialize database before any tests run
from database import Database

@pytest.fixture(scope='session', autouse=True)
def init_test_database():
    """Initialize database schema once before all tests"""
    print("\n[Test Setup] Initializing database schema...")
    Database.init_database()
    print("[Test Setup] Database initialized successfully")
    yield
    print("\n[Test Teardown] Tests completed")


@pytest.fixture(scope='session')
def app():
    """Create and configure Flask app for testing (session-scoped)"""
    # Import app - blueprints are already registered at module import time
    from flask_server_modular import app
    
    app.config['TESTING'] = True
    
    yield app


@pytest.fixture(scope='function')
def client(app):
    """Create Flask test client (function-scoped)"""
    with app.test_client() as client:
        yield client


@pytest.fixture(scope='function')
def clean_database():
    """Clean database tables between tests (optional, use where needed)"""
    # Clear all data but keep schema
    conn = Database.get_connection()
    cursor = conn.cursor()
    
    # Get all table names
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")
    tables = cursor.fetchall()
    
    # Delete all rows from each table
    for table in tables:
        table_name = table['name']
        cursor.execute(f'DELETE FROM {table_name}')
    
    conn.commit()
    yield
