import { useState, useEffect } from 'react';
import { Box, Typography, Grid, CircularProgress, Alert } from '@mui/material';
import BoltIcon from '@mui/icons-material/Bolt';
import ElectricBoltIcon from '@mui/icons-material/ElectricBolt';
import PowerIcon from '@mui/icons-material/Power';
import EnergyIcon from '@mui/icons-material/EnergySavingsLeaf';
import DeviceSelector from '../components/dashboard/DeviceSelector';
import MetricsCard from '../components/dashboard/MetricsCard';
import TimeSeriesChart from '../components/dashboard/TimeSeriesChart';
import { getLatestData, getHistoricalData } from '../api/aggregation';

const Dashboard = () => {
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [latestData, setLatestData] = useState(null);
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

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
          {/* Metrics Cards */}
          <Grid container spacing={3} sx={{ mb: 3 }}>
            <Grid item xs={12} sm={6} md={3}>
              <MetricsCard
                title="Voltage"
                value={latestData.voltage?.toFixed(2)}
                unit="V"
                icon={BoltIcon}
                color="primary"
              />
            </Grid>
            <Grid item xs={12} sm={6} md={3}>
              <MetricsCard
                title="Current"
                value={latestData.current?.toFixed(3)}
                unit="A"
                icon={ElectricBoltIcon}
                color="secondary"
              />
            </Grid>
            <Grid item xs={12} sm={6} md={3}>
              <MetricsCard
                title="Power"
                value={latestData.power?.toFixed(2)}
                unit="W"
                icon={PowerIcon}
                color="warning"
              />
            </Grid>
            <Grid item xs={12} sm={6} md={3}>
              <MetricsCard
                title="Energy"
                value={latestData.energy?.toFixed(3)}
                unit="kWh"
                icon={EnergyIcon}
                color="success"
              />
            </Grid>
          </Grid>

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
