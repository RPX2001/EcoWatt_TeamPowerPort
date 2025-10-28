import { useState, useEffect } from 'react';
import { Box, Typography, Alert, Grid, Tabs, Tab } from '@mui/material';
import { useSearchParams } from 'react-router-dom';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import CommandBuilder from '../components/commands/CommandBuilder';
import CommandQueue from '../components/commands/CommandQueue';
import CommandHistory from '../components/commands/CommandHistory';

const Commands = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [searchParams, setSearchParams] = useSearchParams();
  const tabParam = searchParams.get('tab') || 'send';
  
  // Map tab names to indices
  const tabMap = {
    'send': 0,
    'queue': 1,
    'history': 2
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
    const tabNames = ['send', 'queue', 'history'];
    setSearchParams({ tab: tabNames[newValue] });
  };

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Commands
      </Typography>

      {activeTab === 0 && (
        <DeviceSelector 
          selectedDevice={selectedDevice} 
          onDeviceChange={handleDeviceChange} 
        />
      )}

      <Box sx={{ mt: 3 }}>
        <Tabs value={activeTab} onChange={handleTabChange} sx={{ mb: 2 }}>
          <Tab label="Send Command" />
          <Tab label="Command Queue" />
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

        {activeTab === 1 && <CommandQueue />}

        {activeTab === 2 && <CommandHistory />}
      </Box>
    </Box>
  );
};

export default Commands;
