import { Box, Typography, Paper } from '@mui/material';

const Configuration = () => {
  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Configuration
      </Typography>
      <Paper sx={{ p: 3, mt: 2 }}>
        <Typography variant="body1">
          Device configuration management coming soon...
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Features:
        </Typography>
        <ul>
          <li>Sample rate configuration</li>
          <li>Upload interval settings</li>
          <li>Register selection</li>
          <li>Compression settings</li>
          <li>Power threshold configuration</li>
          <li>Configuration history</li>
        </ul>
      </Paper>
    </Box>
  );
};

export default Configuration;
