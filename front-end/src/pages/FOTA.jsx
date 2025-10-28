import { Box, Typography, Paper } from '@mui/material';

const FOTA = () => {
  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Firmware Over-The-Air (FOTA)
      </Typography>
      <Paper sx={{ p: 3, mt: 2 }}>
        <Typography variant="body1">
          Firmware update management coming soon...
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Features:
        </Typography>
        <ul>
          <li>Firmware upload</li>
          <li>Version management</li>
          <li>Update progress tracking</li>
          <li>Rollback capability</li>
          <li>Update history</li>
        </ul>
      </Paper>
    </Box>
  );
};

export default FOTA;
