import { useState, useEffect } from 'react';
import { Box, Typography, Alert, Grid, Tabs, Tab } from '@mui/material';
import { useSearchParams } from 'react-router-dom';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import CommandBuilder from '../components/commands/CommandBuilder';
import CommandHistory from '../components/commands/CommandHistory';

const Commands = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [searchParams, setSearchParams] = useSearchParams();
  const tabParam = searchParams.get('tab') || 'send';
  
  // Map tab names to indices (no queue tab)
  const tabMap = {
    'send': 0,
    'history': 1
  };
  
  const [activeTab, setActiveTab] = useState(tabMap[tabParam] || 0);

  useEffect(() => {
    const newTab = tabMap[tabParam] || 0;
    setActiveTab(newTab);
  }, [tabParam]);

  const handleDeviceChange = (deviceId) => {
    setSelectedDevice(deviceId);
  };

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
    // Update URL parameter
    const tabNames = ['send', 'history'];
    setSearchParams({ tab: tabNames[newValue] });
  };

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Commands
      </Typography>

      <DeviceSelector 
        selectedDevice={selectedDevice} 
        onDeviceChange={handleDeviceChange} 
      />

      <Box sx={{ mt: 3 }}>
        <Tabs value={activeTab} onChange={handleTabChange} sx={{ mb: 2 }}>
          <Tab label="Send Command" />
          <Tab label="Command History" />
        </Tabs>

        {activeTab === 0 && selectedDevice && (
          <Grid container spacing={3}>
            <Grid item xs={12}>
              <CommandBuilder deviceId={selectedDevice} />
            </Grid>
          </Grid>
        )}

        {activeTab === 0 && !selectedDevice && (
          <Alert severity="info">
            Please select a device to send commands.
          </Alert>
        )}

        {activeTab === 1 && <CommandHistory deviceId={selectedDevice} />}
      </Box>
    </Box>
  );
};

export default Commands;
