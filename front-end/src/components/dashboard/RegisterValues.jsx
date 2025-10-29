import { Card, CardContent, Typography, Grid, Box, Chip } from '@mui/material';
import { formatDistanceToNow } from 'date-fns';

const RegisterValues = ({ data, deviceConfig }) => {
  if (!data || !data.registers) {
    return null;
  }

  const { registers, timestamp, metadata } = data;
  
  // Get configured registers from device config
  // Only show registers that are configured to be polled by ESP32
  const configuredRegisterIds = deviceConfig?.config?.registers || [];
  
  // Filter registers to show only configured ones (exact match, case-sensitive)
  const filteredRegisters = Object.entries(registers).filter(([key]) => {
    // Check if this register is in the configured list
    return configuredRegisterIds.includes(key);
  });

  // Format register values based on metadata
  // NOTE: Backend already applies gain correction, so we use value directly
  const formatValue = (registerName, value) => {
    const meta = metadata?.[registerName];
    if (!meta) {
      return value.toFixed(2);
    }

    // Value is already gain-corrected by the backend
    return `${value.toFixed(meta.decimals)} ${meta.unit}`;
  };

  // Get display name from metadata or format register name
  const getDisplayName = (registerName) => {
    const meta = metadata?.[registerName];
    if (meta?.name) {
      return meta.name;
    }
    return registerName.replace(/([A-Z])/g, ' $1').trim();
  };

  const timeAgo = timestamp 
    ? formatDistanceToNow(new Date(timestamp * 1000), { addSuffix: true })
    : 'Unknown';

  return (
    <Card>
      <CardContent>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6">
            Live Inverter Readings
          </Typography>
          <Chip 
            label={`Updated ${timeAgo}`} 
            size="small" 
            color="primary" 
            variant="outlined"
          />
        </Box>

        <Grid container spacing={2}>
          {filteredRegisters.length > 0 ? (
            filteredRegisters.map(([key, value]) => (
              <Grid item xs={12} sm={6} md={4} lg={3} key={key}>
                <Box
                  sx={{
                    p: 2,
                    border: '1px solid',
                    borderColor: 'divider',
                    borderRadius: 1,
                    bgcolor: 'background.paper',
                  }}
                >
                  <Typography variant="caption" color="text.secondary" gutterBottom>
                    {getDisplayName(key)}
                  </Typography>
                  <Typography variant="h6" component="div">
                    {formatValue(key, value)}
                  </Typography>
                </Box>
              </Grid>
            ))
          ) : (
            <Grid item xs={12}>
              <Typography variant="body2" color="text.secondary" align="center">
                No registers configured. Please configure registers in the Configuration tab.
              </Typography>
            </Grid>
          )}
        </Grid>
      </CardContent>
    </Card>
  );
};

export default RegisterValues;
