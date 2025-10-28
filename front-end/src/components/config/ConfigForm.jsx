import { useState } from 'react';
import {
  Box,
  Paper,
  TextField,
  Switch,
  FormControlLabel,
  Button,
  Grid,
  Typography,
  Alert,
  CircularProgress,
  FormGroup,
  Checkbox,
  Divider,
} from '@mui/material';
import { updateConfig } from '../../api/config';

/**
 * Available Modbus Registers from Inverter SIM API
 * Based on API Documentation Section 4: Modbus Data Registers
 */
const AVAILABLE_REGISTERS = [
  { id: 'voltage', name: 'Phase Voltage (Vac1/L1)', address: 0, unit: 'V', gain: 10, readOnly: true },
  { id: 'current', name: 'Phase Current (Iac1/L1)', address: 1, unit: 'A', gain: 10, readOnly: true },
  { id: 'frequency', name: 'Phase Frequency (Fac1/L1)', address: 2, unit: 'Hz', gain: 100, readOnly: true },
  { id: 'vpv1', name: 'PV1 Input Voltage', address: 3, unit: 'V', gain: 10, readOnly: true },
  { id: 'vpv2', name: 'PV2 Input Voltage', address: 4, unit: 'V', gain: 10, readOnly: true },
  { id: 'ipv1', name: 'PV1 Input Current', address: 5, unit: 'A', gain: 10, readOnly: true },
  { id: 'ipv2', name: 'PV2 Input Current', address: 6, unit: 'A', gain: 10, readOnly: true },
  { id: 'temperature', name: 'Inverter Temperature', address: 7, unit: '°C', gain: 10, readOnly: true },
  { id: 'power', name: 'Inverter Output Power (Pac L)', address: 9, unit: 'W', gain: 1, readOnly: true },
];

const ConfigForm = ({ deviceId, currentConfig, onConfigUpdate }) => {
  const [config, setConfig] = useState({
    sampling_interval: currentConfig?.sampling_interval || 2, // Device reads from inverter every 2s
    upload_interval: currentConfig?.upload_interval || 15, // Device uploads to cloud every 15s
    firmware_check_interval: currentConfig?.firmware_check_interval || 60, // Check for firmware updates every 60s
    command_poll_interval: currentConfig?.command_poll_interval || 10, // Check for pending commands every 10s
    config_poll_interval: currentConfig?.config_poll_interval || 5, // Check for config updates every 5s
    compression_enabled: currentConfig?.compression_enabled ?? true,
    registers: currentConfig?.registers || ['voltage', 'current', 'power'],
  });

  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(false);

  const handleInputChange = (field, value) => {
    setConfig((prev) => ({ ...prev, [field]: value }));
    setSuccess(false);
    setError(null);
  };

  const handleRegisterToggle = (register) => {
    setConfig((prev) => {
      const registers = prev.registers.includes(register)
        ? prev.registers.filter((r) => r !== register)
        : [...prev.registers, register];
      return { ...prev, registers };
    });
    setSuccess(false);
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    
    // Validation based on Milestone 4 specifications
    if (config.sampling_interval < 1 || config.sampling_interval > 300) {
      setError('Sampling interval must be between 1 and 300 seconds');
      return;
    }
    
    if (config.upload_interval < 10 || config.upload_interval > 3600) {
      setError('Upload interval must be between 10 and 3600 seconds');
      return;
    }

    if (config.firmware_check_interval < 30 || config.firmware_check_interval > 86400) {
      setError('Firmware check interval must be between 30 and 86400 seconds');
      return;
    }

    if (config.command_poll_interval < 5 || config.command_poll_interval > 300) {
      setError('Command poll interval must be between 5 and 300 seconds');
      return;
    }

    if (config.config_poll_interval < 1 || config.config_poll_interval > 300) {
      setError('Config poll interval must be between 1 and 300 seconds');
      return;
    }
    
    if (config.registers.length === 0) {
      setError('At least one register must be selected');
      return;
    }

    try {
      setLoading(true);
      setError(null);
      setSuccess(false);

      // Send config update following Milestone 4 format
      const configUpdate = {
        sampling_interval: config.sampling_interval,
        upload_interval: config.upload_interval,
        firmware_check_interval: config.firmware_check_interval,
        command_poll_interval: config.command_poll_interval,
        config_poll_interval: config.config_poll_interval,
        compression_enabled: config.compression_enabled,
        registers: config.registers,
      };

      await updateConfig(deviceId, configUpdate);
      
      setSuccess(true);
      if (onConfigUpdate) {
        onConfigUpdate(configUpdate);
      }
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to update configuration');
      console.error('Error updating config:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleReset = () => {
    setConfig({
      sampling_interval: currentConfig?.sampling_interval || 2,
      upload_interval: currentConfig?.upload_interval || 15,
      firmware_check_interval: currentConfig?.firmware_check_interval || 60,
      command_poll_interval: currentConfig?.command_poll_interval || 10,
      config_poll_interval: currentConfig?.config_poll_interval || 5,
      compression_enabled: currentConfig?.compression_enabled ?? true,
      registers: currentConfig?.registers || ['voltage', 'current', 'power'],
    });
    setError(null);
    setSuccess(false);
  };

  return (
    <Paper elevation={2} sx={{ p: 3 }}>
      <Typography variant="h6" gutterBottom>
        Device Configuration
      </Typography>

      {error && (
        <Alert severity="error" sx={{ mb: 2 }}>
          {error}
        </Alert>
      )}

      {success && (
        <Alert severity="success" sx={{ mb: 2 }}>
          Configuration updated successfully! Command queued for device.
        </Alert>
      )}

      <Box component="form" onSubmit={handleSubmit}>
        <Grid container spacing={3}>
          {/* Timing Parameters Section */}
          <Grid item xs={12}>
            <Typography variant="subtitle1" gutterBottom sx={{ fontWeight: 'bold', mt: 1 }}>
              Timing Parameters
            </Typography>
            <Divider sx={{ mb: 2 }} />
          </Grid>

          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              label="Sampling Interval (seconds)"
              type="number"
              value={config.sampling_interval}
              onChange={(e) => handleInputChange('sampling_interval', Number(e.target.value))}
              helperText="Device reads from inverter every N seconds (1-300s)"
              inputProps={{ min: 1, max: 300, step: 1 }}
            />
          </Grid>

          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              label="Upload Interval (seconds)"
              type="number"
              value={config.upload_interval}
              onChange={(e) => handleInputChange('upload_interval', Number(e.target.value))}
              helperText="Device uploads to cloud every N seconds (10-3600s)"
              inputProps={{ min: 10, max: 3600, step: 5 }}
            />
          </Grid>

          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              label="Firmware Check Interval (seconds)"
              type="number"
              value={config.firmware_check_interval}
              onChange={(e) => handleInputChange('firmware_check_interval', Number(e.target.value))}
              helperText="Check for firmware updates every N seconds (30-86400s)"
              inputProps={{ min: 30, max: 86400, step: 10 }}
            />
          </Grid>

          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              label="Command Poll Interval (seconds)"
              type="number"
              value={config.command_poll_interval}
              onChange={(e) => handleInputChange('command_poll_interval', Number(e.target.value))}
              helperText="Check for pending commands every N seconds (5-300s)"
              inputProps={{ min: 5, max: 300, step: 5 }}
            />
          </Grid>

          <Grid item xs={12} md={6}>
            <TextField
              fullWidth
              label="Config Poll Interval (seconds)"
              type="number"
              value={config.config_poll_interval}
              onChange={(e) => handleInputChange('config_poll_interval', Number(e.target.value))}
              helperText="Check for config updates every N seconds (1-300s)"
              inputProps={{ min: 1, max: 300, step: 1 }}
            />
          </Grid>

          {/* Data Processing Section */}
          <Grid item xs={12}>
            <Typography variant="subtitle1" gutterBottom sx={{ fontWeight: 'bold', mt: 2 }}>
              Data Processing
            </Typography>
            <Divider sx={{ mb: 2 }} />
          </Grid>

          <Grid item xs={12} md={6}>
            <FormControlLabel
              control={
                <Switch
                  checked={config.compression_enabled}
                  onChange={(e) => handleInputChange('compression_enabled', e.target.checked)}
                />
              }
              label="Enable Data Compression"
            />
          </Grid>

          {/* Registers Section */}
          <Grid item xs={12}>
            <Typography variant="subtitle1" gutterBottom sx={{ fontWeight: 'bold', mt: 2 }}>
              Modbus Registers to Monitor
            </Typography>
            <Typography variant="caption" color="text.secondary" gutterBottom>
              Based on Inverter SIM API - Section 4: Modbus Data Registers
            </Typography>
            <Divider sx={{ mb: 2 }} />
          </Grid>

          <Grid item xs={12}>
            <FormGroup>
              {AVAILABLE_REGISTERS.map((register) => (
                <FormControlLabel
                  key={register.id}
                  control={
                    <Checkbox
                      checked={config.registers.includes(register.id)}
                      onChange={() => handleRegisterToggle(register.id)}
                    />
                  }
                  label={`${register.name} (Address ${register.address}, ${register.unit}, Gain: ${register.gain})`}
                />
              ))}
            </FormGroup>
          </Grid>

          <Grid item xs={12}>
            <Box sx={{ display: 'flex', gap: 2, mt: 2 }}>
              <Button
                type="submit"
                variant="contained"
                disabled={loading}
                startIcon={loading ? <CircularProgress size={20} /> : null}
              >
                {loading ? 'Applying...' : 'Apply Configuration'}
              </Button>
              <Button variant="outlined" onClick={handleReset} disabled={loading}>
                Reset
              </Button>
            </Box>
          </Grid>
        </Grid>
      </Box>
    </Paper>
  );
};

export default ConfigForm;
