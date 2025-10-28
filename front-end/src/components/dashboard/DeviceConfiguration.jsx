import { Card, CardContent, Typography, Grid, Box, Chip } from '@mui/material';
import SettingsIcon from '@mui/icons-material/Settings';

const DeviceConfiguration = ({ config }) => {
  if (!config || !config.config) {
    return null;
  }

  const params = config.config;

  // Format time intervals for display
  const formatInterval = (ms, unit = 's') => {
    if (!ms) return 'Default';
    if (unit === 's') {
      return `${ms} s`;
    } else if (unit === 'ms') {
      return `${ms} ms`;
    }
    return ms;
  };

  return (
    <Card>
      <CardContent>
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
          <SettingsIcon color="primary" />
          <Typography variant="h6">
            Device Configuration
          </Typography>
          <Chip 
            label={config.device_id || 'Active'} 
            size="small" 
            color="success" 
            variant="outlined"
          />
        </Box>

        <Grid container spacing={2}>
          {/* Sampling Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Sampling Interval
              </Typography>
              <Typography variant="h6">
                {formatInterval(params.sampling_interval)}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                How often device reads from inverter
              </Typography>
            </Box>
          </Grid>

          {/* Upload Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Upload Interval
              </Typography>
              <Typography variant="h6">
                {formatInterval(params.upload_interval)}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                How often data is sent to cloud
              </Typography>
            </Box>
          </Grid>

          {/* Firmware Check Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Firmware Check Interval
              </Typography>
              <Typography variant="h6">
                {formatInterval(params.firmware_check_interval)}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                OTA update check frequency
              </Typography>
            </Box>
          </Grid>

          {/* Command Poll Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Command Poll Interval
              </Typography>
              <Typography variant="h6">
                {formatInterval(params.command_poll_interval)}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                Check for pending commands
              </Typography>
            </Box>
          </Grid>

          {/* Config Poll Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Config Poll Interval
              </Typography>
              <Typography variant="h6">
                {formatInterval(params.config_poll_interval)}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                Check for config updates
              </Typography>
            </Box>
          </Grid>

          {/* Compression Enabled */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Compression
              </Typography>
              <Typography variant="h6">
                {params.compression_enabled !== false ? 'Enabled' : 'Disabled'}
              </Typography>
              <Typography variant="caption" color="text.secondary">
                Data compression status
              </Typography>
            </Box>
          </Grid>

          {/* Active Registers */}
          <Grid item xs={12}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary" gutterBottom>
                Monitored Registers
              </Typography>
              <Box sx={{ display: 'flex', gap: 1, flexWrap: 'wrap', mt: 1 }}>
                {params.registers && params.registers.length > 0 ? (
                  params.registers.map((reg) => (
                    <Chip 
                      key={reg} 
                      label={reg} 
                      size="small" 
                      color="primary" 
                      variant="outlined"
                    />
                  ))
                ) : (
                  <Typography variant="body2" color="text.secondary">
                    All registers
                  </Typography>
                )}
              </Box>
            </Box>
          </Grid>
        </Grid>
      </CardContent>
    </Card>
  );
};

export default DeviceConfiguration;
