import { useState, useEffect } from 'react';
import { 
  Box, Typography, Grid, CircularProgress, Alert, 
  Paper, Table, TableBody, TableCell, TableContainer, 
  TableHead, TableRow, Tabs, Tab 
} from '@mui/material';
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
  const [viewMode, setViewMode] = useState(0); // 0 = Charts, 1 = Table

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
      
      // Transform historical data to chart format
      const rawHistoricalData = historicalResponse.data.data || [];
      const transformedData = rawHistoricalData.map(record => {
        const registers = record.registers || {};
        return {
          timestamp: record.timestamp,
          // Map register names to chart keys
          voltage: registers.Vac1 ? registers.Vac1 / 10 : 0, // Assuming gain of 10
          current: registers.Iac1 ? registers.Iac1 / 10 : 0,
          power: registers.Pac || 0,
          frequency: registers.Fac1 ? registers.Fac1 / 100 : 0,
          vpv1: registers.Vpv1 ? registers.Vpv1 / 10 : 0,
          vpv2: registers.Vpv2 ? registers.Vpv2 / 10 : 0,
          ipv1: registers.Ipv1 ? registers.Ipv1 / 10 : 0,
          ipv2: registers.Ipv2 ? registers.Ipv2 / 10 : 0,
          temperature: registers.Temperature ? registers.Temperature / 10 : 0,
          exportPowerPct: registers.ExportPowerPct || 0
        };
      });
      
      setHistoricalData(transformedData);
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

          {/* Historical Data Section with Tabs */}
          <Box sx={{ mt: 3 }}>
            <Paper sx={{ p: 2 }}>
              <Tabs value={viewMode} onChange={(e, newValue) => setViewMode(newValue)} sx={{ mb: 2 }}>
                <Tab label="Charts View" />
                <Tab label="Table View" />
              </Tabs>

              {/* Charts View */}
              {viewMode === 0 && (
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
                      title="Power & Temperature Over Time"
                      data={historicalData}
                      dataKeys={[
                        { dataKey: 'power', name: 'Power (W)' },
                        { dataKey: 'temperature', name: 'Temperature (°C)' },
                      ]}
                      colors={['#ff9800', '#f44336']}
                      height={350}
                      yAxisLabel="Value"
                    />
                  </Grid>
                  <Grid item xs={12}>
                    <TimeSeriesChart
                      title="PV Voltage & Current Over Time"
                      data={historicalData}
                      dataKeys={[
                        { dataKey: 'vpv1', name: 'PV1 Voltage (V)' },
                        { dataKey: 'vpv2', name: 'PV2 Voltage (V)' },
                        { dataKey: 'ipv1', name: 'PV1 Current (A)' },
                        { dataKey: 'ipv2', name: 'PV2 Current (A)' },
                      ]}
                      colors={['#4caf50', '#8bc34a', '#ff9800', '#ffc107']}
                      height={350}
                      yAxisLabel="Value"
                    />
                  </Grid>
                </Grid>
              )}

              {/* Table View */}
              {viewMode === 1 && (
                <TableContainer sx={{ maxHeight: 600 }}>
                  <Table stickyHeader>
                    <TableHead>
                      <TableRow>
                        <TableCell>Timestamp</TableCell>
                        <TableCell align="right">Voltage (V)</TableCell>
                        <TableCell align="right">Current (A)</TableCell>
                        <TableCell align="right">Power (W)</TableCell>
                        <TableCell align="right">Frequency (Hz)</TableCell>
                        <TableCell align="right">PV1 V (V)</TableCell>
                        <TableCell align="right">PV1 I (A)</TableCell>
                        <TableCell align="right">PV2 V (V)</TableCell>
                        <TableCell align="right">PV2 I (A)</TableCell>
                        <TableCell align="right">Temp (°C)</TableCell>
                        <TableCell align="right">Export %</TableCell>
                      </TableRow>
                    </TableHead>
                    <TableBody>
                      {historicalData.length === 0 ? (
                        <TableRow>
                          <TableCell colSpan={11} align="center">
                            <Typography color="text.secondary" sx={{ py: 4 }}>
                              No historical data available
                            </Typography>
                          </TableCell>
                        </TableRow>
                      ) : (
                        historicalData.map((row, index) => (
                          <TableRow key={index} hover>
                            <TableCell>
                              {new Date(row.timestamp).toLocaleString()}
                            </TableCell>
                            <TableCell align="right">{row.voltage.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.current.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.power.toFixed(0)}</TableCell>
                            <TableCell align="right">{row.frequency.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.vpv1.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.ipv1.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.vpv2.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.ipv2.toFixed(2)}</TableCell>
                            <TableCell align="right">{row.temperature.toFixed(1)}</TableCell>
                            <TableCell align="right">{row.exportPowerPct.toFixed(0)}</TableCell>
                          </TableRow>
                        ))
                      )}
                    </TableBody>
                  </Table>
                </TableContainer>
              )}
            </Paper>
          </Box>
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
