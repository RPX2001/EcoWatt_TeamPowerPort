import { useState, useEffect } from 'react';
import { Box, Typography, Alert, Tabs, Tab } from '@mui/material';
import { useSearchParams } from 'react-router-dom';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import ConfigForm from '../components/config/ConfigForm';
import ConfigHistory from '../components/config/ConfigHistory';

const Configuration = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [currentConfig, setCurrentConfig] = useState(null);
  const [searchParams, setSearchParams] = useSearchParams();
  const tabParam = searchParams.get('tab') || 'configure';
  
  // Map tab names to indices
  const tabMap = {
    'configure': 0,
    'history': 1
  };
  
  const [activeTab, setActiveTab] = useState(tabMap[tabParam] || 0);

  useEffect(() => {
    const newTab = tabMap[tabParam] || 0;
    setActiveTab(newTab);
  }, [tabParam]);

  const handleDeviceChange = (deviceId) => {
    setSelectedDevice(deviceId);
    // In a real implementation, fetch current config here
    // For now, use defaults
    setCurrentConfig({
      sample_rate_hz: 10,
      upload_interval_s: 60,
      compression_enabled: true,
      power_threshold_w: 1000,
      registers: ['voltage', 'current', 'power', 'energy'],
    });
  };

  const handleConfigUpdate = (newConfig) => {
    setCurrentConfig(newConfig);
  };

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
    // Update URL parameter
    const tabNames = ['configure', 'history'];
    setSearchParams({ tab: tabNames[newValue] });
  };

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Configuration
      </Typography>

      {activeTab === 0 && (
        <DeviceSelector 
          selectedDevice={selectedDevice} 
          onDeviceChange={handleDeviceChange} 
        />
      )}

      <Box sx={{ mt: 3 }}>
        <Tabs value={activeTab} onChange={handleTabChange} sx={{ mb: 2 }}>
          <Tab label="Configure Device" />
          <Tab label="Configuration History" />
        </Tabs>

        {activeTab === 0 && selectedDevice && (
          <ConfigForm
            deviceId={selectedDevice}
            currentConfig={currentConfig}
            onConfigUpdate={handleConfigUpdate}
          />
        )}

        {activeTab === 0 && !selectedDevice && (
          <Alert severity="info">
            Please select a device to configure.
          </Alert>
        )}

        {activeTab === 1 && <ConfigHistory />}
      </Box>
    </Box>
  );
};

export default Configuration;
