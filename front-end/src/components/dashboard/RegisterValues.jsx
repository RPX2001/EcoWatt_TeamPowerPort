import { Card, CardContent, Typography, Grid, Box, Chip } from '@mui/material';
import { formatDistanceToNow } from 'date-fns';

const RegisterValues = ({ data }) => {
  if (!data || !data.registers) {
    return null;
  }

  const { registers, timestamp, metadata } = data;

  // Format register values based on metadata
  const formatValue = (registerName, value) => {
    const meta = metadata?.[registerName];
    if (!meta) {
      return value.toFixed(2);
    }

    const actualValue = value / meta.gain;
    return `${actualValue.toFixed(meta.decimals)} ${meta.unit}`;
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
          {Object.entries(registers).map(([key, value]) => (
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
          ))}
        </Grid>
      </CardContent>
    </Card>
  );
};

export default RegisterValues;
