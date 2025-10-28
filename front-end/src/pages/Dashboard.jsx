import { useState, useEffect, useMemo } from 'react';
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

  // Categorize registers for grouping in charts (MOVED BEFORE USAGE)
  const getCategoryForRegister = (registerName) => {
    const name = registerName.toLowerCase();
    if (name.includes('vac') || name.includes('iac') || name.includes('fac')) {
      return 'ac_power';
    } else if (name.includes('vpv') || name.includes('ipv')) {
      return 'pv_input';
    } else if (name.includes('pac') || name.includes('power')) {
      return 'power';
    } else if (name.includes('temp')) {
      return 'temperature';
    }
    return 'other';
  };

  // Dynamically determine available registers from latest data
  const availableRegisters = useMemo(() => {
    if (!latestData?.registers) return [];
    
    // Get all register names that have values
    return Object.keys(latestData.registers).filter(regName => 
      latestData.registers[regName] !== null && 
      latestData.registers[regName] !== undefined
    );
  }, [latestData]);

  // Create dynamic register mapping with metadata
  const registerConfig = useMemo(() => {
    if (!latestData?.metadata || !availableRegisters.length) return [];
    
    return availableRegisters.map(regName => {
      const metadata = latestData.metadata[regName];
      if (!metadata) return null;
      
      return {
        key: regName,
        name: metadata.name,
        unit: metadata.unit,
        gain: metadata.gain,
        decimals: metadata.decimals,
        chartKey: regName.toLowerCase(),
        category: getCategoryForRegister(regName)
      };
    }).filter(Boolean);
  }, [latestData, availableRegisters]);

  // Group registers by category for chart organization
  const registersByCategory = useMemo(() => {
    const grouped = {
      ac_power: [],
      pv_input: [],
      power: [],
      temperature: [],
      other: []
    };
    
    registerConfig.forEach(reg => {
      grouped[reg.category].push(reg);
    });
    
    return grouped;
  }, [registerConfig]);

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

      // Fetch latest data (may fail if device is offline)
      let latestResponse = null;
      try {
        latestResponse = await getLatestData(selectedDevice);
        setLatestData(latestResponse.data);
      } catch (latestErr) {
        console.warn('Failed to fetch latest data (device may be offline):', latestErr);
        setLatestData(null);
      }

      // Fetch device configuration
      try {
        const configResponse = await getConfig(selectedDevice);
        setDeviceConfig(configResponse.data);
      } catch (configErr) {
        console.warn('Failed to fetch device config:', configErr);
      }

      // Fetch historical data (last 1 hour) - works even if device is offline
      const endTime = new Date();
      const startTime = new Date(endTime.getTime() - 60 * 60 * 1000); // 1 hour ago
      
      const historicalResponse = await getHistoricalData(selectedDevice, {
        start_time: startTime.toISOString(),
        end_time: endTime.toISOString(),
      });
      
      // Transform historical data to chart format dynamically
      const rawHistoricalData = historicalResponse.data.data || [];
      const transformedData = rawHistoricalData.map(record => {
        const registers = record.registers || {};
        const transformed = { timestamp: record.timestamp };
        
        // Dynamically add all available registers with proper scaling
        Object.keys(registers).forEach(regName => {
          // Try to get metadata from latest response, fallback to historical record or default
          let metadata = latestResponse?.data?.metadata?.[regName];
          
          if (!metadata) {
            // Fallback: infer metadata from register name (common patterns)
            // This allows historical data to display even when device is offline
            metadata = { gain: 1, decimals: 2 }; // Safe default
            
            // Apply common scaling patterns
            if (regName.toLowerCase().includes('voltage') || regName.toLowerCase().includes('vac') || regName.toLowerCase().includes('vpv')) {
              metadata.gain = 10; // 0.1V resolution
            } else if (regName.toLowerCase().includes('current') || regName.toLowerCase().includes('iac') || regName.toLowerCase().includes('ipv')) {
              metadata.gain = 10; // 0.1A resolution
            }
          }
          
          if (registers[regName] !== null) {
            const rawValue = registers[regName];
            const scaledValue = rawValue / metadata.gain;
            transformed[regName.toLowerCase()] = scaledValue;
          }
        });
        
        return transformed;
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

      {selectedDevice && (
        <>
          {/* Show active device metrics if available */}
          {latestData && (
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

              {/* Metrics Cards - Dynamic based on available registers */}
              {registerConfig.length > 0 && (
                <Grid container spacing={3} sx={{ mb: 3 }}>
                  {registerConfig.map((reg, index) => {
                    const rawValue = latestData?.registers?.[reg.key];
                    
                    if (rawValue === undefined || rawValue === null) {
                      return null;
                    }

                    const actualValue = rawValue / reg.gain;
                    const displayValue = actualValue.toFixed(reg.decimals);

                    return (
                      <Grid item xs={12} sm={6} md={4} lg={3} key={reg.key}>
                        <MetricsCard
                          title={reg.name}
                          value={displayValue}
                          unit={reg.unit}
                          icon={getIconForRegister(reg.key)}
                          color={getColorForRegister(index)}
                        />
                      </Grid>
                    );
                  })}
                </Grid>
              )}
            </>
          )}

          {/* Show offline message if no latest data */}
          {!latestData && !loading && (
            <Alert severity="info" sx={{ mb: 3 }}>
              Device is currently offline. Showing historical data from database.
            </Alert>
          )}
          {/* Historical Data Section with Tabs */}
          <Box sx={{ mt: 3 }}>
            <Paper sx={{ p: 2 }}>
              <Tabs value={viewMode} onChange={(e, newValue) => setViewMode(newValue)} sx={{ mb: 2 }}>
                <Tab label="Charts View" />
                <Tab label="Table View" />
              </Tabs>

              {/* Charts View - Dynamic based on available register categories */}
              {viewMode === 0 && (
                <Grid container spacing={3}>
                  {/* AC Power Metrics Chart */}
                  {registersByCategory.ac_power.length > 0 && (
                    <Grid item xs={12}>
                      <TimeSeriesChart
                        title="AC Power Metrics"
                        data={historicalData}
                        dataKeys={registersByCategory.ac_power.map(reg => ({
                          dataKey: reg.chartKey,
                          name: `${reg.name} (${reg.unit})`
                        }))}
                        colors={['#1976d2', '#dc004e', '#4caf50', '#ff9800']}
                        height={350}
                        yAxisLabel="Value"
                      />
                    </Grid>
                  )}

                  {/* PV Input Metrics Chart */}
                  {registersByCategory.pv_input.length > 0 && (
                    <Grid item xs={12}>
                      <TimeSeriesChart
                        title="PV Input Metrics"
                        data={historicalData}
                        dataKeys={registersByCategory.pv_input.map(reg => ({
                          dataKey: reg.chartKey,
                          name: `${reg.name} (${reg.unit})`
                        }))}
                        colors={['#4caf50', '#8bc34a', '#ff9800', '#ffc107']}
                        height={350}
                        yAxisLabel="Value"
                      />
                    </Grid>
                  )}

                  {/* Power & Temperature Chart */}
                  {(registersByCategory.power.length > 0 || registersByCategory.temperature.length > 0) && (
                    <Grid item xs={12}>
                      <TimeSeriesChart
                        title="Power & Temperature"
                        data={historicalData}
                        dataKeys={[
                          ...registersByCategory.power.map(reg => ({
                            dataKey: reg.chartKey,
                            name: `${reg.name} (${reg.unit})`
                          })),
                          ...registersByCategory.temperature.map(reg => ({
                            dataKey: reg.chartKey,
                            name: `${reg.name} (${reg.unit})`
                          }))
                        ]}
                        colors={['#ff9800', '#f44336', '#9c27b0']}
                        height={350}
                        yAxisLabel="Value"
                      />
                    </Grid>
                  )}

                  {/* Other Metrics Chart */}
                  {registersByCategory.other.length > 0 && (
                    <Grid item xs={12}>
                      <TimeSeriesChart
                        title="Other Metrics"
                        data={historicalData}
                        dataKeys={registersByCategory.other.map(reg => ({
                          dataKey: reg.chartKey,
                          name: `${reg.name} (${reg.unit})`
                        }))}
                        colors={['#9c27b0', '#3f51b5', '#009688']}
                        height={350}
                        yAxisLabel="Value"
                      />
                    </Grid>
                  )}

                  {/* Show message if no data */}
                  {registerConfig.length === 0 && (
                    <Grid item xs={12}>
                      <Alert severity="info">
                        No register data available to display charts
                      </Alert>
                    </Grid>
                  )}
                </Grid>
              )}

              {/* Table View - Dynamic columns based on available registers */}
              {viewMode === 1 && (
                <TableContainer sx={{ maxHeight: 600 }}>
                  <Table stickyHeader>
                    <TableHead>
                      <TableRow>
                        <TableCell>Timestamp</TableCell>
                        {registerConfig.map(reg => (
                          <TableCell key={reg.key} align="right">
                            {reg.name} ({reg.unit})
                          </TableCell>
                        ))}
                      </TableRow>
                    </TableHead>
                    <TableBody>
                      {historicalData.length === 0 ? (
                        <TableRow>
                          <TableCell colSpan={registerConfig.length + 1} align="center">
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
                            {registerConfig.map(reg => {
                              const value = row[reg.chartKey];
                              return (
                                <TableCell key={reg.key} align="right">
                                  {value !== undefined && value !== null 
                                    ? value.toFixed(reg.decimals)
                                    : '-'}
                                </TableCell>
                              );
                            })}
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
