import { useState, useEffect } from 'react';
import {
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  CircularProgress,
  Alert,
  Box,
} from '@mui/material';
import { getDevices } from '../../api/devices';

const DeviceSelector = ({ selectedDevice, onDeviceChange }) => {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    fetchDevices();
  }, []);

  const fetchDevices = async () => {
    try {
      setLoading(true);
      setError(null);
      const response = await getDevices();
      console.log('DeviceSelector - Full API Response:', response);
      console.log('DeviceSelector - Devices array:', response.data.devices);
      console.log('DeviceSelector - Devices count:', response.data.devices?.length);
      setDevices(response.data.devices || []);
      
      // Auto-select first device if none selected
      if (!selectedDevice && response.data.devices?.length > 0) {
        console.log('DeviceSelector - Auto-selecting device:', response.data.devices[0].device_id);
        onDeviceChange(response.data.devices[0].device_id);
      }
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to fetch devices');
      console.error('Error fetching devices:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleChange = (event) => {
    onDeviceChange(event.target.value);
  };

  if (loading) {
    return (
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
        <CircularProgress size={24} />
        <span>Loading devices...</span>
      </Box>
    );
  }

  if (error) {
    return (
      <Alert severity="error" sx={{ mb: 2 }}>
        {error}
      </Alert>
    );
  }

  if (devices.length === 0) {
    return (
      <Alert severity="info" sx={{ mb: 2 }}>
        No devices registered. Please register a device first.
      </Alert>
    );
  }

  return (
    <FormControl fullWidth sx={{ mb: 3 }}>
      <InputLabel id="device-selector-label">Select Device</InputLabel>
      <Select
        labelId="device-selector-label"
        id="device-selector"
        value={selectedDevice || ''}
        label="Select Device"
        onChange={handleChange}
      >
        {devices.map((device) => (
          <MenuItem key={device.device_id} value={device.device_id}>
            {device.device_name || device.device_id}
            {device.status && ` (${device.status})`}
          </MenuItem>
        ))}
      </Select>
    </FormControl>
  );
};

export default DeviceSelector;
