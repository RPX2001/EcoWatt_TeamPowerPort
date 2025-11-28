import { useState } from 'react';
import {
  Box,
  Paper,
  TextField,
  Button,
  Grid,
  Typography,
  Alert,
  CircularProgress,
  MenuItem,
  Select,
  FormControl,
  InputLabel,
  Divider,
} from '@mui/material';
import { sendCommand } from '../../api/commands';

// M4-Compliant Register Map (matches Flask REGISTER_NAME_MAP)
const REGISTER_MAP = [
  { name: 'export_power', label: 'Export Power (%)', address: 8, writable: true, range: [0, 100], description: 'Inverter export power percentage (0-100%)' },
  { name: 'status_flag', label: 'Status Flag', address: 0, writable: false, description: 'Device status indicators' },
  { name: 'battery_voltage', label: 'Battery Voltage', address: 1, writable: false, description: 'Battery voltage (V)' },
  { name: 'battery_current', label: 'Battery Current', address: 2, writable: false, description: 'Battery current (A)' },
  { name: 'pv_voltage', label: 'PV Voltage', address: 3, writable: false, description: 'Photovoltaic voltage (V)' },
  { name: 'pv_current', label: 'PV Current', address: 4, writable: false, description: 'Photovoltaic current (A)' },
  { name: 'load_voltage', label: 'Load Voltage', address: 5, writable: false, description: 'Load voltage (V)' },
  { name: 'load_current', label: 'Load Current', address: 6, writable: false, description: 'Load current (A)' },
  { name: 'temperature', label: 'Temperature', address: 7, writable: false, description: 'Device temperature (°C)' },
  { name: 'grid_frequency', label: 'Grid Frequency', address: 9, writable: false, description: 'Grid frequency (Hz)' },
];

/**
 * CommandBuilder Component
 * Sends commands to EcoWatt Device following Milestone 4 Command Execution Protocol
 * Primary focus: Write Register 8 (Export Power Percentage)
 * Writable Register: Address 8 - Export power percentage (0-100%)
 */
const CommandBuilder = ({ deviceId }) => {
  const [commandType, setCommandType] = useState('write_register');
  const [selectedRegister, setSelectedRegister] = useState('export_power'); // M4: Use string register name
  const [registerValue, setRegisterValue] = useState(50); // Value for selected register
  const [parameters, setParameters] = useState({
    export_power_percent: 50, // Legacy field for backward compatibility
    register_address: 8, // Numeric address (auto-synced with selectedRegister)
    register_value: 50,
  });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);

  // Get selected register details
  const selectedRegisterInfo = REGISTER_MAP.find(r => r.name === selectedRegister) || REGISTER_MAP[0];

  // Command types based on Milestone 4 specification
  const commandTypes = [
    { 
      value: 'write_register', 
      label: 'Write Register (Inverter Control)', 
      description: 'Write to Modbus register (Primary: Register 8 - Export Power %)',
      params: ['register_address', 'register_value'] 
    },
    { 
      value: 'set_power', 
      label: 'Set Power Limit (Generic)', 
      description: 'Set power limit in watts (legacy/generic command)',
      params: ['power_w'] 
    },
    { 
      value: 'update_config', 
      label: 'Trigger Config Update (Generic)', 
      description: 'Request immediate configuration update',
      params: [] 
    },
  ];

  const handleCommandTypeChange = (type) => {
    setCommandType(type);
    setError(null);
    setSuccess(null);
  };

  const handleParameterChange = (param, value) => {
    setParameters((prev) => ({ ...prev, [param]: value }));
    setError(null);
    setSuccess(null);
  };

  const handleSubmit = async (e) => {
    e.preventDefault();

    // Validation for selected register
    if (commandType === 'write_register') {
      // Check if register is writable
      if (!selectedRegisterInfo.writable) {
        setError(`Register "${selectedRegisterInfo.label}" is read-only. Only Export Power (register 8) is writable.`);
        return;
      }
      
      // Validate value range
      if (selectedRegisterInfo.range) {
        const [min, max] = selectedRegisterInfo.range;
        if (registerValue < min || registerValue > max) {
          setError(`${selectedRegisterInfo.label} must be between ${min} and ${max}`);
          return;
        }
      }
    }

    try {
      setLoading(true);
      setError(null);
      setSuccess(null);

      // Build command payload following Milestone 4 format
      // { "command": { "action": "...", "target_register": "...", "value": ... } }
      
      const action = commandType;
      let targetRegister = null;
      let value = null;

      if (commandType === 'write_register') {
        // M4: Use string register name directly (Flask will map to address)
        targetRegister = selectedRegister;  // e.g., "export_power"
        value = registerValue;  // Use dedicated state instead of parameters
      } else if (commandType === 'set_power') {
        // Legacy command - use 'power' as target_register for consistency
        targetRegister = 'power';
        value = parameters.power_w;
      }

      // Send using sendCommand(deviceId, action, targetRegister, value)
      const response = await sendCommand(deviceId, action, targetRegister, value);
      
      setSuccess(`✓ Command queued successfully! Will be executed at next communication window. Command ID: ${response.data.command_id || 'N/A'}`);
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to send command');
      console.error('Error sending command:', err);
    } finally {
      setLoading(false);
    }
  };

  const selectedCommandType = commandTypes.find((cmd) => cmd.value === commandType);

  return (
    <Paper elevation={2} sx={{ p: 3 }}>
      <Typography variant="h6" gutterBottom>
        Command Builder
      </Typography>
      <Typography variant="caption" color="text.secondary" gutterBottom display="block">
        Commands are queued and executed at the next scheduled communication window
      </Typography>

      {error && (
        <Alert severity="error" sx={{ mb: 2 }}>
          {error}
        </Alert>
      )}

      {success && (
        <Alert severity="success" sx={{ mb: 2 }}>
          {success}
        </Alert>
      )}

      <Box component="form" onSubmit={handleSubmit}>
        <Grid container spacing={3}>
          <Grid item xs={12}>
            <FormControl fullWidth>
              <InputLabel>Command Type</InputLabel>
              <Select
                value={commandType}
                label="Command Type"
                onChange={(e) => handleCommandTypeChange(e.target.value)}
              >
                {commandTypes.map((cmd) => (
                  <MenuItem key={cmd.value} value={cmd.value}>
                    {cmd.label}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
            {selectedCommandType && (
              <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>
                {selectedCommandType.description}
              </Typography>
            )}
          </Grid>

          <Grid item xs={12}>
            <Divider />
          </Grid>

          {/* Write Register Parameters - M4 Format with Register Name Selection */}
          {selectedCommandType && selectedCommandType.params.includes('register_address') && (
            <>
              <Grid item xs={12}>
                <Typography variant="subtitle2" gutterBottom>
                  Register Write Parameters
                </Typography>
                <Typography variant="caption" color="primary" display="block" sx={{ mb: 2 }}>
                  <strong>Writable Register:</strong> Export Power (register 8, 0-100%)
                  <br />
                  Read-only registers: Status, Voltages, Currents, Temperature, Frequency
                </Typography>
              </Grid>
              
              {/* Register Selection Dropdown - M4 Compliant */}
              <Grid item xs={12} md={6}>
                <FormControl fullWidth>
                  <InputLabel>Target Register</InputLabel>
                  <Select
                    value={selectedRegister}
                    label="Target Register"
                    onChange={(e) => {
                      const newRegister = e.target.value;
                      setSelectedRegister(newRegister);
                      const regInfo = REGISTER_MAP.find(r => r.name === newRegister);
                      if (regInfo) {
                        // Sync legacy parameters for backward compatibility
                        handleParameterChange('register_address', regInfo.address);
                        // Set default value based on range
                        if (regInfo.range) {
                          const defaultValue = Math.floor((regInfo.range[0] + regInfo.range[1]) / 2);
                          setRegisterValue(defaultValue);
                          handleParameterChange('register_value', defaultValue);
                        }
                      }
                    }}
                  >
                    {REGISTER_MAP.map((reg) => (
                      <MenuItem key={reg.name} value={reg.name} disabled={!reg.writable}>
                        {reg.label} (Addr: {reg.address}) {!reg.writable && '- Read Only'}
                      </MenuItem>
                    ))}
                  </Select>
                </FormControl>
                <Typography variant="caption" color="text.secondary" sx={{ mt: 0.5, display: 'block' }}>
                  {selectedRegisterInfo.description}
                </Typography>
              </Grid>
              
              {/* Register Value Input */}
              <Grid item xs={12} md={6}>
                <TextField
                  fullWidth
                  label={`${selectedRegisterInfo.label} Value`}
                  type="number"
                  value={registerValue}
                  onChange={(e) => {
                    const newValue = Number(e.target.value);
                    setRegisterValue(newValue);
                    handleParameterChange('register_value', newValue); // Sync legacy
                  }}
                  helperText={
                    selectedRegisterInfo.range 
                      ? `Range: ${selectedRegisterInfo.range[0]} - ${selectedRegisterInfo.range[1]}`
                      : 'Value to write'
                  }
                  inputProps={{
                    min: selectedRegisterInfo.range ? selectedRegisterInfo.range[0] : 0,
                    max: selectedRegisterInfo.range ? selectedRegisterInfo.range[1] : 65535,
                    step: 1
                  }}
                  disabled={!selectedRegisterInfo.writable}
                  error={!selectedRegisterInfo.writable}
                />
              </Grid>
            </>
          )}

          {/* Generic Power Command */}
          {selectedCommandType && selectedCommandType.params.includes('power_w') && (
            <Grid item xs={12}>
              <TextField
                fullWidth
                label="Power Limit (W)"
                type="number"
                value={parameters.power_w}
                onChange={(e) => handleParameterChange('power_w', Number(e.target.value))}
                helperText="Set the power limit in watts (generic command)"
                inputProps={{ min: 0, step: 10 }}
              />
            </Grid>
          )}

          <Grid item xs={12}>
            <Button
              type="submit"
              variant="contained"
              disabled={loading || !deviceId}
              startIcon={loading ? <CircularProgress size={20} /> : null}
              fullWidth
            >
              {loading ? 'Queueing Command...' : 'Queue Command'}
            </Button>
          </Grid>
        </Grid>
      </Box>
    </Paper>
  );
};

export default CommandBuilder;
