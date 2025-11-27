/**
 * Logs & Diagnostics Page
 * 
 * Main page for viewing diagnostic logs and system information
 */

import React, { useState } from 'react';
import {
  Box,
  Typography,
  Paper,
  Tabs,
  Tab,
  Alert
} from '@mui/material';
import {
  HealthAndSafety as HealthIcon,
  Devices as DeviceLogIcon,
  Storage as ServerLogIcon
} from '@mui/icons-material';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import DiagnosticsSummary from '../components/diagnostics/DiagnosticsSummary';
import LogViewer from '../components/diagnostics/LogViewer';
import ServerLogViewer from '../components/diagnostics/ServerLogViewer';

const Logs = () => {
  const [activeTab, setActiveTab] = useState(0);
  const [selectedDevice, setSelectedDevice] = useState(null);

  const handleDeviceChange = (deviceId) => {
    setSelectedDevice(deviceId);
  };

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
  };

  return (
    <Box sx={{ width: '100%', py: 4, px: 3 }}>
      <Box sx={{ mb: 4 }}>
        <Typography variant="h4" component="h1" gutterBottom>
          Logs & Diagnostics
        </Typography>
        <Typography variant="body1" color="text.secondary">
          View device diagnostics, system health, and server logs
        </Typography>
      </Box>

      <DeviceSelector 
        selectedDevice={selectedDevice} 
        onDeviceChange={handleDeviceChange} 
      />

      <Paper sx={{ mb: 3 }}>
        <Tabs 
          value={activeTab} 
          onChange={handleTabChange}
          variant="fullWidth"
          sx={{ borderBottom: 1, borderColor: 'divider' }}
        >
          <Tab 
            icon={<DeviceLogIcon />} 
            label="Device Logs"
            iconPosition="start"
          />
          <Tab 
            icon={<HealthIcon />} 
            label="System Health"
            iconPosition="start"
          />
          <Tab 
            icon={<ServerLogIcon />} 
            label="Server Logs"
            iconPosition="start"
          />
        </Tabs>
      </Paper>

      <Box>
        {activeTab === 0 && (
          <LogViewer deviceId={selectedDevice} />
        )}

        {activeTab === 1 && (
          <DiagnosticsSummary deviceId={selectedDevice} />
        )}

        {activeTab === 2 && (
          <ServerLogViewer />
        )}
      </Box>
    </Box>
  );
};

export default Logs;
