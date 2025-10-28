import { Box, Typography, Paper } from '@mui/material';

const Commands = () => {
  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Commands
      </Typography>
      <Paper sx={{ p: 3, mt: 2 }}>
        <Typography variant="body1">
          Command execution interface coming soon...
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
          Features:
        </Typography>
        <ul>
          <li>Command builder (set_power, write_register, update_config)</li>
          <li>Command queue viewer</li>
          <li>Command history with status</li>
          <li>Response viewer</li>
        </ul>
      </Paper>
    </Box>
  );
};

export default Commands;
