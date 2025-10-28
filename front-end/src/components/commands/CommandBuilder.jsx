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

/**
 * CommandBuilder Component
 * Sends commands to EcoWatt Device following Milestone 4 Command Execution Protocol
 * Primary focus: Write Register 8 (Export Power Percentage)
 * Writable Register: Address 8 - Export power percentage (0-100%)
 */
const CommandBuilder = ({ deviceId }) => {
  const [commandType, setCommandType] = useState('write_register');
  const [parameters, setParameters] = useState({
    export_power_percent: 50, // Register 8: Export power percentage (0-100%)
    register_address: 8, // Focus on writable register
    register_value: 50,
  });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);

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

    // Validation for Register 8 (Export Power Percentage)
    if (commandType === 'write_register') {
      if (parameters.register_address === 8) {
        if (parameters.register_value < 0 || parameters.register_value > 100) {
          setError('Export power percentage must be between 0 and 100');
          return;
        }
      }
    }

    try {
      setLoading(true);
      setError(null);
      setSuccess(null);

      // Build command payload following Milestone 4 format
      let commandParams = {};
      const selectedCommand = commandTypes.find((cmd) => cmd.value === commandType);

      if (selectedCommand) {
        selectedCommand.params.forEach((param) => {
          commandParams[param] = parameters[param];
        });
      }

      // Command payload following Milestone 4 specification:
      // { "command": { "action": "write_register", "target_register": "status_flag", "value": 1 } }
      const commandPayload = {
        action: commandType,
        ...commandParams,
      };

      const response = await sendCommand(deviceId, commandPayload);
      
      setSuccess(`Command queued successfully! Will be executed at next communication window. Command ID: ${response.data.command_id || 'N/A'}`);
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

          {/* Write Register Parameters - Primary: Register 8 (Export Power %) */}
          {selectedCommandType && selectedCommandType.params.includes('register_address') && (
            <>
              <Grid item xs={12}>
                <Typography variant="subtitle2" gutterBottom>
                  Register Write Parameters
                </Typography>
                <Typography variant="caption" color="primary" display="block" sx={{ mb: 2 }}>
                  <strong>Register 8 (Writable):</strong> Export Power Percentage (0-100%)
                  <br />
                  Other registers (0-7, 9) are read-only
                </Typography>
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  fullWidth
                  label="Register Address"
                  type="number"
                  value={parameters.register_address}
                  onChange={(e) => handleParameterChange('register_address', Number(e.target.value))}
                  helperText="Modbus register address (8 = Export Power %)"
                  inputProps={{ min: 0, max: 9, step: 1 }}
                />
              </Grid>
              <Grid item xs={12} md={6}>
                <TextField
                  fullWidth
                  label="Register Value"
                  type="number"
                  value={parameters.register_value}
                  onChange={(e) => handleParameterChange('register_value', Number(e.target.value))}
                  helperText={parameters.register_address === 8 ? "Export power % (0-100)" : "Value to write"}
                  inputProps={{ min: 0, max: parameters.register_address === 8 ? 100 : 65535, step: 1 }}
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
