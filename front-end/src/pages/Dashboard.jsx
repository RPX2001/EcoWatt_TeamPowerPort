import { useState, useEffect } from 'react';
import { Box, Typography, Grid, CircularProgress, Alert } from '@mui/material';
import BoltIcon from '@mui/icons-material/Bolt';
import ElectricBoltIcon from '@mui/icons-material/ElectricBolt';
import PowerIcon from '@mui/icons-material/Power';
import EnergyIcon from '@mui/icons-material/EnergySavingsLeaf';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import MetricsCard from '../components/dashboard/MetricsCard';
import RegisterValues from '../components/dashboard/RegisterValues';
import DeviceConfiguration from '../components/dashboard/DeviceConfiguration';
import TimeSeriesChart from '../components/dashboard/TimeSeriesChart';
import { getLatestData, getHistoricalData } from '../api/aggregation';
import { getConfig } from '../api/config';

const Dashboard = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [latestData, setLatestData] = useState(null);
  const [deviceConfig, setDeviceConfig] = useState(null);
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  // Icon mapping for different register types
  const getIconForRegister = (registerName) => {
    if (registerName.toLowerCase().includes('voltage') || registerName.toLowerCase().includes('vac') || registerName.toLowerCase().includes('vpv')) {
      return BoltIcon;
    } else if (registerName.toLowerCase().includes('current') || registerName.toLowerCase().includes('iac') || registerName.toLowerCase().includes('ipv')) {
      return ElectricBoltIcon;
    } else if (registerName.toLowerCase().includes('power') || registerName.toLowerCase().includes('pac')) {
      return PowerIcon;
    } else {
      return EnergyIcon;
    }
  };

  // Color mapping for different register types
  const getColorForRegister = (index) => {
    const colors = ['primary', 'secondary', 'warning', 'success', 'info', 'error'];
    return colors[index % colors.length];
  };

  useEffect(() => {
    if (selectedDevice) {
      fetchData();
      // Set up auto-refresh every 5 seconds
      const interval = setInterval(fetchData, 5000);
      return () => clearInterval(interval);
    }
  }, [selectedDevice]);

  const fetchData = async () => {
    if (!selectedDevice) return;

    try {
      setLoading(true);
      setError(null);

      // Fetch latest data
      const latestResponse = await getLatestData(selectedDevice);
      setLatestData(latestResponse.data);

      // Fetch device configuration
      try {
        const configResponse = await getConfig(selectedDevice);
        setDeviceConfig(configResponse.data);
      } catch (configErr) {
        console.warn('Failed to fetch device config:', configErr);
      }

      // Fetch historical data (last 1 hour)
      const endTime = new Date();
      const startTime = new Date(endTime.getTime() - 60 * 60 * 1000); // 1 hour ago
      
      const historicalResponse = await getHistoricalData(selectedDevice, {
        start_time: startTime.toISOString(),
        end_time: endTime.toISOString(),
      });
      
      setHistoricalData(historicalResponse.data.data || []);
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to fetch data');
      console.error('Error fetching data:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleDeviceChange = (deviceId) => {
    setSelectedDevice(deviceId);
    setLatestData(null);
    setHistoricalData([]);
  };

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Dashboard
      </Typography>

      <DeviceSelector 
        selectedDevice={selectedDevice} 
        onDeviceChange={handleDeviceChange} 
      />

      {error && (
        <Alert severity="error" sx={{ mb: 2 }}>
          {error}
        </Alert>
      )}

      {loading && !latestData && (
        <Box sx={{ display: 'flex', justifyContent: 'center', my: 4 }}>
          <CircularProgress />
        </Box>
      )}

      {selectedDevice && latestData && (
        <>
          {/* Register Values - Full width */}
          <Box sx={{ mb: 3 }}>
            <RegisterValues data={latestData} />
          </Box>

          {/* Device Configuration - Full width */}
          {deviceConfig && (
            <Box sx={{ mb: 3 }}>
              <DeviceConfiguration config={deviceConfig} />
            </Box>
          )}

          {/* Metrics Cards - Only for Monitored Registers */}
          {deviceConfig?.config?.registers && deviceConfig.config.registers.length > 0 && (
            <Grid container spacing={3} sx={{ mb: 3 }}>
              {deviceConfig.config.registers.map((registerKey, index) => {
                // Map register key to actual register name in latestData
                const registerNameMap = {
                  'voltage': 'Vac1',
                  'current': 'Iac1',
                  'frequency': 'Fac1',
                  'vpv1': 'Vpv1',
                  'vpv2': 'Vpv2',
                  'ipv1': 'Ipv1',
                  'ipv2': 'Ipv2',
                  'temperature': 'Temperature',
                  'export_power_pct': 'ExportPowerPct',
                  'power': 'Pac'
                };

                const registerName = registerNameMap[registerKey] || registerKey;
                const metadata = latestData?.metadata?.[registerName];
                const rawValue = latestData?.registers?.[registerName];

                if (!metadata || rawValue === undefined) {
                  return null;
                }

                const actualValue = rawValue / metadata.gain;
                const displayValue = actualValue.toFixed(metadata.decimals);

                return (
                  <Grid item xs={12} sm={6} md={4} lg={3} key={registerKey}>
                    <MetricsCard
                      title={metadata.name}
                      value={displayValue}
                      unit={metadata.unit}
                      icon={getIconForRegister(registerName)}
                      color={getColorForRegister(index)}
                    />
                  </Grid>
                );
              })}
            </Grid>
          )}

          {/* Time Series Charts */}
          <Grid container spacing={3}>
            <Grid item xs={12}>
              <TimeSeriesChart
                title="Voltage & Current Over Time"
                data={historicalData}
                dataKeys={[
                  { dataKey: 'voltage', name: 'Voltage (V)' },
                  { dataKey: 'current', name: 'Current (A)' },
                ]}
                colors={['#1976d2', '#dc004e']}
                height={350}
                yAxisLabel="Value"
              />
            </Grid>
            <Grid item xs={12}>
              <TimeSeriesChart
                title="Power & Energy Over Time"
                data={historicalData}
                dataKeys={[
                  { dataKey: 'power', name: 'Power (W)' },
                  { dataKey: 'energy', name: 'Energy (kWh)' },
                ]}
                colors={['#ff9800', '#4caf50']}
                height={350}
                yAxisLabel="Value"
              />
            </Grid>
          </Grid>
        </>
      )}

      {!selectedDevice && (
        <Alert severity="info">
          Please select a device to view the dashboard.
        </Alert>
      )}
    </Box>
  );
};

export default Dashboard;
