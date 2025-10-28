import { Card, CardContent, Typography, Grid, Box, Chip } from '@mui/material';
import SettingsIcon from '@mui/icons-material/Settings';

const DeviceConfiguration = ({ config }) => {
  if (!config || !config.config_params) {
    return null;
  }

  const params = config.config_params;

  return (
    <Card>
      <CardContent>
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
          <SettingsIcon color="primary" />
          <Typography variant="h6">
            Device Configuration
          </Typography>
          <Chip 
            label={config.version || 'v1.0'} 
            size="small" 
            color="success" 
            variant="outlined"
          />
        </Box>

        <Grid container spacing={2}>
          {/* Polling Interval */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Polling Interval
              </Typography>
              <Typography variant="h6">
                {params.polling_interval_ms ? `${params.polling_interval_ms} ms` : 'Default'}
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
                {params.upload_interval_ms ? `${params.upload_interval_ms} ms` : 'Default'}
              </Typography>
            </Box>
          </Grid>

          {/* Batch Size */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Batch Size
              </Typography>
              <Typography variant="h6">
                {params.batch_size || 'Auto'}
              </Typography>
            </Box>
          </Grid>

          {/* Compression Method */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Compression
              </Typography>
              <Typography variant="h6">
                {params.compression_method || 'Smart Selection'}
              </Typography>
            </Box>
          </Grid>

          {/* Security Enabled */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Security Layer
              </Typography>
              <Typography variant="h6">
                {params.security_enabled !== false ? 'Enabled' : 'Disabled'}
              </Typography>
            </Box>
          </Grid>

          {/* Active Registers */}
          <Grid item xs={12} sm={6} md={4}>
            <Box sx={{ p: 2, border: '1px solid', borderColor: 'divider', borderRadius: 1 }}>
              <Typography variant="caption" color="text.secondary">
                Active Registers
              </Typography>
              <Typography variant="h6">
                {params.active_register_count || 'All'}
              </Typography>
            </Box>
          </Grid>
        </Grid>
      </CardContent>
    </Card>
  );
};

export default DeviceConfiguration;
