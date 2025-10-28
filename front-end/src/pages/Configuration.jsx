import { useState, useEffect } from 'react';
import { Box, Typography, Alert, Tabs, Tab, CircularProgress } from '@mui/material';
import { useSearchParams } from 'react-router-dom';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import ConfigForm from '../components/config/ConfigForm';
import ConfigHistory from '../components/config/ConfigHistory';
import { getConfig } from '../api/config';

const Configuration = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [currentConfig, setCurrentConfig] = useState(null);
  const [isDefaultConfig, setIsDefaultConfig] = useState(true);
  const [loading, setLoading] = useState(false);
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

  useEffect(() => {
    if (selectedDevice) {
      fetchCurrentConfig();
    }
  }, [selectedDevice]);

  const fetchCurrentConfig = async () => {
    try {
      setLoading(true);
      const response = await getConfig(selectedDevice);
      setCurrentConfig(response.data.config);
      setIsDefaultConfig(response.data.is_default || false);
    } catch (error) {
      console.error('Failed to fetch config:', error);
      // Set default config if fetch fails
      setCurrentConfig({
        sampling_interval: 2,
        upload_interval: 15,
        firmware_check_interval: 60,
        command_poll_interval: 10,
        config_poll_interval: 5,
        compression_enabled: true,
        registers: ['voltage', 'current', 'power']
      });
      setIsDefaultConfig(true);
    } finally {
      setLoading(false);
    }
  };

  const handleDeviceChange = (deviceId) => {
    setSelectedDevice(deviceId);
    setCurrentConfig(null);
    setIsDefaultConfig(true);
  };

  const handleConfigUpdate = (newConfig) => {
    setCurrentConfig(newConfig);
    setIsDefaultConfig(false);
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

      <DeviceSelector 
        selectedDevice={selectedDevice} 
        onDeviceChange={handleDeviceChange} 
      />

      {isDefaultConfig && selectedDevice && activeTab === 0 && (
        <Alert severity="info" sx={{ mt: 2 }}>
          This device is using default configuration. Submit changes to override defaults.
        </Alert>
      )}

      <Box sx={{ mt: 3 }}>
        <Tabs value={activeTab} onChange={handleTabChange} sx={{ mb: 2 }}>
          <Tab label="Configure Device" />
          <Tab label="Configuration History" />
        </Tabs>

        {activeTab === 0 && selectedDevice && loading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', my: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {activeTab === 0 && selectedDevice && !loading && currentConfig && (
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

        {activeTab === 1 && <ConfigHistory deviceId={selectedDevice} />}
      </Box>
    </Box>
  );
};

export default Configuration;
