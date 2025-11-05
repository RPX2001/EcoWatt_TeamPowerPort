import { useState, useEffect } from 'react';
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
  Chip,
  Tooltip,
} from '@mui/material';
import { updateConfig } from '../../api/config';
import { getPowerConfig, updatePowerConfig, encodeTechniques, decodeTechniques, TECHNIQUE_NAMES, TECHNIQUE_DESCRIPTIONS } from '../../api/power';

/**
 * Available Modbus Registers from Inverter SIM API
 * Based on API Documentation Section 4: Modbus Data Registers
 * Using actual register names as stored in database
 */
const AVAILABLE_REGISTERS = [
  { id: 'Vac1', name: 'Vac1 /L1 Phase voltage', address: 0, unit: 'V', gain: 10 },
  { id: 'Iac1', name: 'Iac1 /L1 Phase current', address: 1, unit: 'A', gain: 10 },
  { id: 'Fac1', name: 'Fac1 /L1 Phase frequency', address: 2, unit: 'Hz', gain: 100 },
  { id: 'Vpv1', name: 'Vpv1 /PV1 input voltage', address: 3, unit: 'V', gain: 10 },
  { id: 'Vpv2', name: 'Vpv2 /PV2 input voltage', address: 4, unit: 'V', gain: 10 },
  { id: 'Ipv1', name: 'Ipv1 /PV1 input current', address: 5, unit: 'A', gain: 10 },
  { id: 'Ipv2', name: 'Ipv2 /PV2 input current', address: 6, unit: 'A', gain: 10 },
  { id: 'Temp', name: 'Inverter internal temperature', address: 7, unit: '°C', gain: 10 },
  { id: 'Pow', name: 'Set the export power percentage', address: 8, unit: '%', gain: 1, writable: true },
  { id: 'Pac', name: 'Pac L /Inverter current output power', address: 9, unit: 'W', gain: 1 },
];

const ConfigForm = ({ deviceId, currentConfig, onConfigUpdate }) => {
  const [config, setConfig] = useState({
    sampling_interval: 2,
    upload_interval: 15,
    firmware_check_interval: 60,
    command_poll_interval: 10,
    config_poll_interval: 5,
    compression_enabled: true,
    registers: ['Vac1', 'Iac1', 'Pac'],
  });

  const [powerConfig, setPowerConfig] = useState({
    enabled: false,
    techniques: [],
    energy_poll_interval: 300, // 300 seconds (5 minutes)
  });

  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(false);
  const [powerLoading, setPowerLoading] = useState(false);

  // Update local state when currentConfig changes
  useEffect(() => {
    if (currentConfig) {
      setConfig({
        sampling_interval: currentConfig.sampling_interval ?? 2,
        upload_interval: currentConfig.upload_interval ?? 15,
        firmware_check_interval: currentConfig.firmware_check_interval ?? 60,
        command_poll_interval: currentConfig.command_poll_interval ?? 10,
        config_poll_interval: currentConfig.config_poll_interval ?? 5,
        compression_enabled: currentConfig.compression_enabled ?? true,
        registers: currentConfig.registers ?? ['voltage', 'current', 'power'],
      });

      // Extract power management from main config if present
      if (currentConfig.power_management) {
        // Decode techniques bitmask to array of technique names
        const techniquesList = currentConfig.power_management.techniques_list || 
                               decodeTechniques(currentConfig.power_management.techniques || 0);
        
        setPowerConfig({
          enabled: currentConfig.power_management.enabled ?? false,
          techniques: techniquesList,
          energy_poll_interval: currentConfig.energy_poll_interval ?? 300,
        });
      } else if (currentConfig.energy_poll_interval) {
        // If energy_poll_interval is present but power_management isn't, update it
        setPowerConfig(prev => ({
          ...prev,
          energy_poll_interval: currentConfig.energy_poll_interval
        }));
      }
    }
  }, [currentConfig]);

  // Fetch power configuration when device changes (fallback if not in main config)
  useEffect(() => {
    if (deviceId && !currentConfig?.power_management) {
      fetchPowerConfig();
    }
  }, [deviceId, currentConfig]);

  const fetchPowerConfig = async () => {
    try {
      setPowerLoading(true);
      const response = await getPowerConfig(deviceId);
      const pm = response.data.power_management;
      
      // Decode techniques bitmask to array of technique names
      const techniquesList = pm.techniques_list || decodeTechniques(pm.techniques || 0);
      
      setPowerConfig({
        enabled: pm.enabled,
        techniques: techniquesList,
        energy_poll_interval: pm.energy_poll_interval ?? 300,
      });
    } catch (err) {
      console.error('Failed to fetch power config:', err);
      // Use defaults on error
      setPowerConfig({
        enabled: false,
        techniques: [],
        energy_poll_interval: 300,
      });
    } finally {
      setPowerLoading(false);
    }
  };

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

  const handlePowerToggle = (field, value) => {
    setPowerConfig((prev) => ({ ...prev, [field]: value }));
    setSuccess(false);
    setError(null);
  };

  const handleTechniqueToggle = (technique) => {
    setPowerConfig((prev) => {
      const techniques = prev.techniques.includes(technique)
        ? prev.techniques.filter((t) => t !== technique)
        : [...prev.techniques, technique];
      return { ...prev, techniques };
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

    if (powerConfig.energy_poll_interval < 60 || powerConfig.energy_poll_interval > 3600) {
      setError('Energy poll interval must be between 60 and 3600 seconds');
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

      // Send config update with power management integrated
      const techniquesBitmask = encodeTechniques(powerConfig.techniques);
      const configUpdate = {
        sampling_interval: config.sampling_interval,
        upload_interval: config.upload_interval,
        firmware_check_interval: config.firmware_check_interval,
        command_poll_interval: config.command_poll_interval,
        config_poll_interval: config.config_poll_interval,
        compression_enabled: config.compression_enabled,
        registers: config.registers,
        energy_poll_interval: powerConfig.energy_poll_interval,
        power_management: {
          enabled: powerConfig.enabled,
          techniques: techniquesBitmask,
        },
      };

      console.log('[ConfigForm] Sending config update:', configUpdate);
      console.log('[ConfigForm] Power management:', configUpdate.power_management);
      
      await updateConfig(deviceId, configUpdate);
      
      // Update local state to reflect the changes immediately
      setConfig(prev => ({
        ...prev,
        sampling_interval: configUpdate.sampling_interval,
        upload_interval: configUpdate.upload_interval,
        firmware_check_interval: configUpdate.firmware_check_interval,
        command_poll_interval: configUpdate.command_poll_interval,
        config_poll_interval: configUpdate.config_poll_interval,
        compression_enabled: configUpdate.compression_enabled,
        registers: configUpdate.registers,
      }));
      
      setPowerConfig(prev => ({
        ...prev,
        enabled: configUpdate.power_management.enabled,
        techniques: powerConfig.techniques, // Keep the list form
        energy_poll_interval: configUpdate.energy_poll_interval,
      }));
      
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
    if (currentConfig) {
      setConfig({
        sampling_interval: currentConfig.sampling_interval ?? 2,
        upload_interval: currentConfig.upload_interval ?? 15,
        firmware_check_interval: currentConfig.firmware_check_interval ?? 60,
        command_poll_interval: currentConfig.command_poll_interval ?? 10,
        config_poll_interval: currentConfig.config_poll_interval ?? 5,
        compression_enabled: currentConfig.compression_enabled ?? true,
        registers: currentConfig.registers ?? ['voltage', 'current', 'power'],
      });
    }
    // Refetch power config to reset to server values
    fetchPowerConfig();
    setError(null);
    setSuccess(false);
  };

  return (
    <Paper elevation={2} sx={{ p: 4, maxWidth: 1200 }}>
      <Typography variant="h5" gutterBottom sx={{ mb: 3 }}>
        Device Configuration
      </Typography>

      {error && (
        <Alert severity="error" sx={{ mb: 3 }}>
          {error}
        </Alert>
      )}

      {success && (
        <Alert severity="success" sx={{ mb: 3 }}>
          Configuration updated successfully! Command queued for device.
        </Alert>
      )}

      <Box component="form" onSubmit={handleSubmit}>
        {/* Timing Parameters Section */}
        <Box sx={{ mb: 4 }}>
          <Typography variant="h6" gutterBottom sx={{ fontWeight: 'bold', mb: 2 }}>
            ⏱️ Timing Parameters
          </Typography>
          <Divider sx={{ mb: 3 }} />
          
          <Grid container spacing={3}>
            <Grid item xs={12} md={6}>
              <TextField
                fullWidth
                label="Sampling Interval"
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
                label="Upload Interval"
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
                label="Firmware Check Interval"
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
                label="Command Poll Interval"
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
                label="Config Poll Interval"
                type="number"
                value={config.config_poll_interval}
                onChange={(e) => handleInputChange('config_poll_interval', Number(e.target.value))}
                helperText="Check for config updates every N seconds (1-300s)"
                inputProps={{ min: 1, max: 300, step: 1 }}
              />
            </Grid>
          </Grid>
        </Box>

        {/* Data Processing Section */}
        <Box sx={{ mb: 4 }}>
          <Typography variant="h6" gutterBottom sx={{ fontWeight: 'bold', mb: 2 }}>
            🗜️ Data Processing
          </Typography>
          <Divider sx={{ mb: 3 }} />
          
          <FormControlLabel
            control={
              <Switch
                checked={config.compression_enabled}
                onChange={(e) => handleInputChange('compression_enabled', e.target.checked)}
              />
            }
            label={
              <Box>
                <Typography variant="body1">Enable Data Compression</Typography>
                <Typography variant="caption" color="text.secondary">
                  Compress data before uploading to reduce bandwidth usage
                </Typography>
              </Box>
            }
          />
        </Box>

        {/* Registers Section */}
        <Box sx={{ mb: 4 }}>
          <Typography variant="h6" gutterBottom sx={{ fontWeight: 'bold', mb: 1 }}>
            📊 Modbus Registers to Monitor
          </Typography>
          <Typography variant="body2" color="text.secondary" gutterBottom sx={{ mb: 2 }}>
            Based on Inverter SIM API - Section 4: Modbus Data Registers
          </Typography>
          <Divider sx={{ mb: 3 }} />
          
          <Grid container spacing={2}>
            {AVAILABLE_REGISTERS.map((register) => (
              <Grid item xs={12} sm={6} md={4} key={register.id}>
                <FormControlLabel
                  control={
                    <Checkbox
                      checked={config.registers.includes(register.id)}
                      onChange={() => handleRegisterToggle(register.id)}
                    />
                  }
                  label={
                    <Tooltip title={`Address: ${register.address}, Unit: ${register.unit}, Gain: ${register.gain}${register.writable ? ' (Writable)' : ''}`}>
                      <Typography variant="body2">
                        {register.name}
                      </Typography>
                    </Tooltip>
                  }
                  sx={{ width: '100%' }}
                />
              </Grid>
            ))}
          </Grid>
        </Box>

        {/* Power Management Section */}
        <Box sx={{ mb: 4 }}>
          <Typography variant="h6" gutterBottom sx={{ fontWeight: 'bold', mb: 1 }}>
            ⚡ Power Management
          </Typography>
          <Typography variant="body2" color="text.secondary" gutterBottom sx={{ mb: 2 }}>
            Configure power-saving techniques to reduce energy consumption
          </Typography>
          <Divider sx={{ mb: 3 }} />
          
          <Grid container spacing={3}>
            <Grid item xs={12}>
              <FormControlLabel
                control={
                  <Switch
                    checked={powerConfig.enabled}
                    onChange={(e) => handlePowerToggle('enabled', e.target.checked)}
                    disabled={powerLoading}
                  />
                }
                label={
                  <Box>
                    <Typography variant="body1">Enable Power Management</Typography>
                    <Typography variant="caption" color="text.secondary">
                      Activate power-saving techniques on the device
                    </Typography>
                  </Box>
                }
              />
            </Grid>

            <Grid item xs={12} md={6}>
              <TextField
                fullWidth
                label="Energy Report Frequency"
                type="number"
                value={powerConfig.energy_poll_interval}
                onChange={(e) => handlePowerToggle('energy_poll_interval', Number(e.target.value))}
              helperText="How often device reports energy statistics (60-3600 s, always active)"
              inputProps={{ min: 60, max: 3600, step: 60 }}
              disabled={powerLoading}
            />
          </Grid>

          <Grid item xs={12}>
            <Typography variant="body2" gutterBottom sx={{ fontWeight: 'bold', mt: 2 }}>
              Power Saving Techniques
            </Typography>
            <Grid container spacing={2}>
              <Grid item xs={12} sm={6}>
                <Tooltip title={TECHNIQUE_DESCRIPTIONS.wifi_modem_sleep} arrow placement="right">
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={powerConfig.techniques.includes('wifi_modem_sleep')}
                        onChange={() => handleTechniqueToggle('wifi_modem_sleep')}
                        disabled={powerLoading || !powerConfig.enabled}
                      />
                    }
                    label={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Typography variant="body2">{TECHNIQUE_NAMES.wifi_modem_sleep}</Typography>
                        <Chip label="Active" size="small" color="success" variant="outlined" />
                      </Box>
                    }
                    sx={{ width: '100%' }}
                  />
                </Tooltip>
              </Grid>

              <Grid item xs={12} sm={6}>
                <Tooltip title={TECHNIQUE_DESCRIPTIONS.cpu_freq_scaling} arrow placement="right">
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={powerConfig.techniques.includes('cpu_freq_scaling')}
                        onChange={() => handleTechniqueToggle('cpu_freq_scaling')}
                        disabled={powerLoading || !powerConfig.enabled}
                      />
                    }
                    label={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Typography variant="body2">{TECHNIQUE_NAMES.cpu_freq_scaling}</Typography>
                        <Chip label="Future" size="small" color="warning" variant="outlined" />
                      </Box>
                    }
                    sx={{ width: '100%' }}
                  />
                </Tooltip>
              </Grid>

              <Grid item xs={12} sm={6}>
                <Tooltip title={TECHNIQUE_DESCRIPTIONS.light_sleep} arrow placement="right">
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={powerConfig.techniques.includes('light_sleep')}
                        onChange={() => handleTechniqueToggle('light_sleep')}
                        disabled={powerLoading || !powerConfig.enabled}
                      />
                    }
                    label={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Typography variant="body2">{TECHNIQUE_NAMES.light_sleep}</Typography>
                        <Chip label="Future" size="small" color="warning" variant="outlined" />
                      </Box>
                    }
                    sx={{ width: '100%' }}
                  />
                </Tooltip>
              </Grid>

              <Grid item xs={12} sm={6}>
                <Tooltip title={TECHNIQUE_DESCRIPTIONS.peripheral_gating} arrow placement="right">
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={powerConfig.techniques.includes('peripheral_gating')}
                        onChange={() => handleTechniqueToggle('peripheral_gating')}
                        disabled={powerLoading || !powerConfig.enabled}
                      />
                    }
                    label={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Typography variant="body2">{TECHNIQUE_NAMES.peripheral_gating}</Typography>
                        <Chip label="Future" size="small" color="warning" variant="outlined" />
                      </Box>
                    }
                    sx={{ width: '100%' }}
                  />
                </Tooltip>
              </Grid>
            </Grid>
          </Grid>
        </Grid>
        </Box>

        {/* Action Buttons */}
        <Box sx={{ display: 'flex', gap: 2, mt: 3, justifyContent: 'flex-end' }}>
          <Button variant="outlined" onClick={handleReset} disabled={loading}>
            Reset
          </Button>
          <Button
            type="submit"
            variant="contained"
            disabled={loading}
            startIcon={loading ? <CircularProgress size={20} /> : null}
          >
            {loading ? 'Applying...' : 'Apply Configuration'}
          </Button>
        </Box>
      </Box>
    </Paper>
  );
};

export default ConfigForm;
