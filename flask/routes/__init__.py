"""
Routes package for EcoWatt Flask server
Provides Flask Blueprint endpoints that use handlers for business logic
"""

# Import all route blueprints
from .diagnostics_routes import diagnostics_bp
from .security_routes import security_bp
from .aggregation_routes import aggregation_bp
from .ota_routes import ota_bp
from .command_routes import command_bp
from .fault_routes import fault_bp
from .general_routes import general_bp

# Export all blueprints
__all__ = [
    'diagnostics_bp',
    'security_bp',
    'aggregation_bp',
    'ota_bp',
    'command_bp',
    'fault_bp',
    'general_bp'
]
