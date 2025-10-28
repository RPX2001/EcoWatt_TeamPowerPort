import { Card, CardContent, Typography, Grid, Box, Chip } from '@mui/material';
import { formatDistanceToNow } from 'date-fns';

const RegisterValues = ({ data }) => {
  if (!data || !data.registers) {
    return null;
  }

  const { registers, timestamp } = data;

  // Format register values for display
  const formatValue = (key, value) => {
    if (key.includes('Voltage')) {
      return `${value.toFixed(2)} V`;
    } else if (key.includes('Current')) {
      return `${value.toFixed(3)} A`;
    } else if (key.includes('Power')) {
      return `${value.toFixed(2)} W`;
    } else if (key.includes('Energy')) {
      return `${value.toFixed(3)} kWh`;
    } else if (key.includes('Frequency')) {
      return `${value.toFixed(2)} Hz`;
    } else if (key.includes('PowerFactor')) {
      return value.toFixed(3);
    } else {
      return value.toFixed(2);
    }
  };

  const timeAgo = timestamp 
    ? formatDistanceToNow(new Date(timestamp * 1000), { addSuffix: true })
    : 'Unknown';

  return (
    <Card>
      <CardContent>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6">
            Current Register Values
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
                  {key.replace(/([A-Z])/g, ' $1').trim()}
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
