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

  // All available registers (complete list of 10 registers)
  const ALL_REGISTERS = [
    'Vac1', 'Iac1', 'Fac1', 'Vpv1', 'Vpv2', 
    'Ipv1', 'Ipv2', 'Temp', 'Pow', 'Pac'
  ];

  // Helper function to format timestamps safely
  const formatTimestamp = (timestamp) => {
    if (!timestamp) return 'N/A';
    try {
      const date = new Date(timestamp);
      return isNaN(date.getTime()) ? 'Invalid Date' : date.toLocaleString();
    } catch (e) {
      return 'Invalid Date';
    }
  };

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

  // Dynamically determine available registers from latest data OR historical data
  // Show ALL registers that have data, regardless of configuration
  const availableRegisters = useMemo(() => {
    // First try to get from latest data (preferred when device is online)
    if (latestData?.registers) {
      // Get all register names that have values
      return Object.keys(latestData.registers).filter(regName => {
        const hasValue = latestData.registers[regName] !== null && 
                         latestData.registers[regName] !== undefined;
        return hasValue;
      });
    }
    
    // Fallback: If device is offline, show ALL 10 possible registers
    // Even if some don't have data in historical records
    if (historicalData.length > 0) {
      // Get unique register keys from historical data
      const historicalKeys = new Set();
      historicalData.forEach(record => {
        Object.keys(record).forEach(key => {
          if (key !== 'timestamp') {
            // Convert lowercase keys back to proper capitalization
            const capitalizedKey = ALL_REGISTERS.find(
              reg => reg.toLowerCase() === key.toLowerCase()
            );
            if (capitalizedKey) {
              historicalKeys.add(capitalizedKey);
            }
          }
        });
      });
      
      // Return ALL registers, not just ones with historical data
      // This ensures all 10 registers are shown even when offline
      return ALL_REGISTERS;
    }
    
    return [];
  }, [latestData, historicalData]);

  // Create dynamic register mapping with metadata
  const registerConfig = useMemo(() => {
    if (!availableRegisters.length) return [];
    
    return availableRegisters.map(regName => {
      // Try to get metadata from latest data first
      let metadata = latestData?.metadata?.[regName];
      
      // If no metadata (device offline), infer from register name
      if (!metadata) {
        metadata = { 
          name: regName, 
          unit: '', 
          gain: 1, 
          decimals: 2 
        };
        
        // Infer common metadata patterns
        const nameLower = regName.toLowerCase();
        if (nameLower.includes('vac') || nameLower.includes('vpv')) {
          metadata.unit = 'V';
          metadata.gain = 10;
          metadata.decimals = 1;
        } else if (nameLower.includes('iac') || nameLower.includes('ipv')) {
          metadata.unit = 'A';
          metadata.gain = 10;
          metadata.decimals = 1;
        } else if (nameLower.includes('pac') || nameLower.includes('power')) {
          metadata.unit = 'W';
          metadata.gain = 1;
          metadata.decimals = 0;
        } else if (nameLower.includes('fac') || nameLower.includes('freq')) {
          metadata.unit = 'Hz';
          metadata.gain = 100;
          metadata.decimals = 2;
        } else if (nameLower.includes('temp')) {
          metadata.unit = '°C';
          metadata.gain = 10;
          metadata.decimals = 1;
        }
      }
      
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

      // Fetch ALL historical data stored in database (no time limit)
      // This allows viewing data even when device is offline
      console.log('Fetching all available historical data for device:', selectedDevice);
      
      const historicalResponse = await getHistoricalData(selectedDevice, {
        limit: 1000, // Get up to 1000 most recent records (increase if needed)
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
            // Backend already applies gain correction, use value directly
            const value = registers[regName];
            transformed[regName.toLowerCase()] = value;
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
                <RegisterValues data={latestData} deviceConfig={deviceConfig} />
              </Box>

              {/* Device Configuration - Full width */}
              {deviceConfig && (
                <Box sx={{ mb: 3 }}>
                  <DeviceConfiguration config={deviceConfig} />
                </Box>
              )}
            </>
          )}

          {/* Show offline message if no latest data but we have historical data */}
          {!latestData && !loading && historicalData.length > 0 && (
            <Alert severity="info" sx={{ mb: 3 }}>
              Device is currently offline. Showing {historicalData.length} historical data points from database (last 24 hours).
            </Alert>
          )}
          
          {/* Show message if device is offline and no data available */}
          {!latestData && !loading && historicalData.length === 0 && (
            <Alert severity="warning" sx={{ mb: 3 }}>
              Device is currently offline and no historical data is available in the database.
              Please check if the device has ever uploaded data to the server.
            </Alert>
          )}
          
          {/* Show info about historical data availability */}
          {historicalData.length > 0 && !latestData && (
            <Alert severity="info" sx={{ mb: 3 }}>
              Device is currently offline. Showing {historicalData.length} historical records from the database.
            </Alert>
          )}
          
          {/* Historical Data Section with Tabs - Only show if we have data */}
          {historicalData.length > 0 && (
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
                <>
                  {registerConfig.length === 0 ? (
                    <Alert severity="info" sx={{ mt: 2 }}>
                      No registers configured. Please configure registers in the Configuration tab to view data.
                    </Alert>
                  ) : (
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
                                  {formatTimestamp(row.timestamp)}
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
                </>
              )}
            </Paper>
          </Box>
          )}
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
