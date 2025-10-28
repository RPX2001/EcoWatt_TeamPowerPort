import { Box, Typography, Paper } from '@mui/material';
import APITester from '../components/utilities/APITester';

const Utilities = () => {
  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Utilities
      </Typography>
      
      <APITester />

      <Paper sx={{ p: 3, mt: 3 }}>
        <Typography variant="body1">
          Data export and utilities coming soon...
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Features:
        </Typography>
        <ul>
          <li>Export data (CSV, JSON)</li>
          <li>Data format preview</li>
          <li>Date range selection</li>
          <li>Download management</li>
        </ul>
      </Paper>
    </Box>
  );
};

export default Utilities;
