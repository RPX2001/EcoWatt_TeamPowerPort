import { useState, useEffect } from 'react';
import { 
  Card, CardContent, Typography, Box, Chip, Grid, LinearProgress, 
  Tooltip, Tabs, Tab, Table, TableBody, TableCell, TableContainer, 
  TableHead, TableRow, Paper, Alert 
} from '@mui/material';
import { CircularProgressbar, buildStyles } from 'react-circular-progressbar';
import 'react-circular-progressbar/dist/styles.css';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip as RechartsTooltip, Legend, ResponsiveContainer } from 'recharts';
import BatteryChargingFullIcon from '@mui/icons-material/BatteryChargingFull';
import EnergySavingsLeafIcon from '@mui/icons-material/EnergySavingsLeaf';
import AccessTimeIcon from '@mui/icons-material/AccessTime';
import { getEnergyHistory, getPowerConfig, TECHNIQUE_NAMES } from '../../api/power';

const PowerWidget = ({ deviceId }) => {
  const [powerData, setPowerData] = useState(null);
  const [powerConfig, setPowerConfig] = useState(null); // Current power configuration
  const [historicalData, setHistoricalData] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [viewMode, setViewMode] = useState(0); // 0 = Summary, 1 = Chart, 2 = Table

  useEffect(() => {
    if (deviceId) {
      fetchPowerData();
      // Refresh every 30 seconds
      const interval = setInterval(fetchPowerData, 30000);
      return () => clearInterval(interval);
    }
  }, [deviceId]);

  const fetchPowerData = async () => {
    try {
      setLoading(true);
      setError(null);
      
      // Fetch current power configuration
      try {
        const configResponse = await getPowerConfig(deviceId);
        if (configResponse.data && configResponse.data.power_management) {
          setPowerConfig(configResponse.data.power_management);
        }
      } catch (err) {
        console.warn('Failed to fetch power config:', err);
        setPowerConfig(null);
      }
      
      // Fetch latest report for summary
      const latestResponse = await getEnergyHistory(deviceId, 'all', 1);
      if (latestResponse.data.reports && latestResponse.data.reports.length > 0) {
        setPowerData(latestResponse.data.reports[0]);
      } else {
        setPowerData(null);
      }

      // Fetch ALL history for charts and table (not just 24h)
      const historyResponse = await getEnergyHistory(deviceId, 'all', 500);
      if (historyResponse.data.reports) {
        setHistoricalData(historyResponse.data.reports);
      } else {
        setHistoricalData([]);
      }
    } catch (err) {
      console.error('Failed to fetch power data:', err);
      setError('Unable to load power data');
      setPowerData(null);
      setHistoricalData([]);
    } finally {
      setLoading(false);
    }
  };

  const formatUptime = (uptimeMs) => {
    if (!uptimeMs) return 'N/A';
    const hours = Math.floor(uptimeMs / 3600000);
    const minutes = Math.floor((uptimeMs % 3600000) / 60000);
    return `${hours}h ${minutes}m`;
  };

  const formatTimestamp = (timestamp) => {
    if (!timestamp) return 'N/A';
    try {
      const date = new Date(timestamp);
      return date.toLocaleString();
    } catch (e) {
      return 'Invalid Date';
    }
  };

  const formatChartData = () => {
    return historicalData.map(report => ({
      timestamp: new Date(report.timestamp).toLocaleTimeString(),
      current: report.avg_current_ma,
      energySaved: report.energy_saved_mah,
      uptime: report.uptime_ms / 3600000, // Convert to hours
    })).reverse(); // Reverse to show oldest first
  };

  const calculateCurrentPercentage = (currentMa) => {
    // Typical ESP32 current range: 80mA (sleep) to 240mA (full power)
    // Map to 0-100% scale
    const minCurrent = 80;
    const maxCurrent = 240;
    const percentage = ((currentMa - minCurrent) / (maxCurrent - minCurrent)) * 100;
    return Math.max(0, Math.min(100, percentage));
  };

  const getColorForCurrent = (percentage) => {
    if (percentage < 30) return '#4caf50'; // Green (low power)
    if (percentage < 70) return '#ff9800'; // Orange (medium)
    return '#f44336'; // Red (high power)
  };

  // Show "No Data" message only if there's truly no data
  if (!powerData && historicalData.length === 0 && !powerConfig) {
    return (
      <Card elevation={2} sx={{ height: '100%' }}>
        <CardContent>
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
            <EnergySavingsLeafIcon color="disabled" />
            <Typography variant="h6" color="text.secondary">
              Power Management
            </Typography>
          </Box>
          <Alert severity="info" sx={{ mt: 2 }}>
            No power data available. Device may not have reported energy statistics yet.
          </Alert>
        </CardContent>
      </Card>
    );
  }

  const currentPercentage = powerData ? calculateCurrentPercentage(powerData.avg_current_ma) : 0;
  const currentColor = getColorForCurrent(currentPercentage);
  const chartData = formatChartData();
  
  // Use config-based status if no report data, otherwise use report data
  const isPowerManagementEnabled = powerData?.enabled ?? powerConfig?.enabled ?? false;
  const activeTechniques = powerData?.techniques_list || powerConfig?.techniques_list || [];

  return (
    <Card elevation={2} sx={{ height: '100%' }}>
      <CardContent>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2 }}>
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
            <EnergySavingsLeafIcon color={isPowerManagementEnabled ? "success" : "disabled"} />
            <Typography variant="h6">
              Power Management
            </Typography>
          </Box>
          <Chip 
            label={isPowerManagementEnabled ? "Active" : "Disabled"} 
            size="small" 
            color={isPowerManagementEnabled ? "success" : "default"} 
          />
        </Box>

        {loading && <LinearProgress sx={{ mb: 2 }} />}

        {/* Tabs for different views */}
        <Tabs value={viewMode} onChange={(e, newValue) => setViewMode(newValue)} sx={{ mb: 2 }}>
          <Tab label="Summary" />
          <Tab label="Chart" />
          <Tab label="Table" />
        </Tabs>

        {/* Summary View */}
        {viewMode === 0 && (
          <>
            {powerData ? (
              <Grid container spacing={3}>
                {/* Current Consumption Gauge */}
                <Grid item xs={12} md={6}>
                  <Box sx={{ textAlign: 'center' }}>
                    <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                      Current Consumption
                    </Typography>
                <Box sx={{ width: 150, height: 150, margin: '0 auto', mb: 2 }}>
                  <CircularProgressbar
                    value={currentPercentage}
                    text={`${powerData.avg_current_ma.toFixed(1)} mA`}
                    styles={buildStyles({
                      textSize: '14px',
                      pathColor: currentColor,
                      textColor: currentColor,
                      trailColor: '#e0e0e0',
                    })}
                  />
                </Box>
                <Typography variant="caption" color="text.secondary">
                  {currentPercentage < 30 ? 'Low Power Mode' : 
                   currentPercentage < 70 ? 'Normal Mode' : 'High Power Mode'}
                </Typography>
              </Box>
            </Grid>

            {/* Energy Saved & Uptime */}
            <Grid item xs={12} md={6}>
              <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                {/* Energy Saved */}
                <Box>
                  <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 1 }}>
                    <BatteryChargingFullIcon color="success" fontSize="small" />
                    <Typography variant="subtitle2" color="text.secondary">
                      Energy Saved
                    </Typography>
                  </Box>
                  <Typography variant="h5" color="success.main" sx={{ fontWeight: 600 }}>
                    {powerData.energy_saved_mah.toFixed(2)} mAh
                  </Typography>
                </Box>

                {/* Uptime */}
                <Box>
                  <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 1 }}>
                    <AccessTimeIcon color="primary" fontSize="small" />
                    <Typography variant="subtitle2" color="text.secondary">
                      Uptime
                    </Typography>
                  </Box>
                  <Typography variant="h6" color="primary.main">
                    {formatUptime(powerData.uptime_ms)}
                  </Typography>
                </Box>

                {/* Active Techniques */}
                <Box>
                  <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                    Active Techniques
                  </Typography>
                  <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 0.5 }}>
                    {isPowerManagementEnabled && activeTechniques && activeTechniques.length > 0 ? (
                      activeTechniques.map((technique) => (
                        <Tooltip key={technique} title={TECHNIQUE_NAMES[technique] || technique} arrow>
                          <Chip
                            label={TECHNIQUE_NAMES[technique] || technique}
                            size="small"
                            color="primary"
                            variant="outlined"
                          />
                        </Tooltip>
                      ))
                    ) : (
                      <Typography variant="caption" color="text.secondary">
                        {isPowerManagementEnabled ? 'No techniques active' : 'Power management disabled'}
                      </Typography>
                    )}
                  </Box>
                </Box>
              </Box>
            </Grid>

            {/* Time Distribution (if available) */}
            {powerData.time_distribution && (
              <Grid item xs={12}>
                <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                  Power Mode Distribution
                </Typography>
                <Box sx={{ mt: 1 }}>
                  {['high_perf_ms', 'normal_ms', 'low_power_ms', 'sleep_ms'].map((mode) => {
                    const value = powerData.time_distribution[mode] || 0;
                    const total = powerData.uptime_ms || 1;
                    const percentage = (value / total) * 100;
                    const label = mode.replace('_ms', '').replace(/_/g, ' ');
                    
                    return (
                      <Box key={mode} sx={{ mb: 1 }}>
                        <Box sx={{ display: 'flex', justifyContent: 'space-between', mb: 0.5 }}>
                          <Typography variant="caption" sx={{ textTransform: 'capitalize' }}>
                            {label}
                          </Typography>
                          <Typography variant="caption" color="text.secondary">
                            {percentage.toFixed(1)}%
                          </Typography>
                        </Box>
                        <LinearProgress 
                          variant="determinate" 
                          value={percentage} 
                          sx={{ height: 6, borderRadius: 1 }}
                        />
                      </Box>
                    );
                  })}
                </Box>
              </Grid>
            )}
          </Grid>
            ) : (
              // Show configuration status even when no energy report data
              <Alert severity={isPowerManagementEnabled ? "success" : "info"} sx={{ mt: 2 }}>
                <Typography variant="subtitle2" gutterBottom>
                  Power Management Status: {isPowerManagementEnabled ? "Enabled" : "Disabled"}
                </Typography>
                {isPowerManagementEnabled && activeTechniques.length > 0 && (
                  <Box sx={{ mt: 1 }}>
                    <Typography variant="caption" color="text.secondary" display="block" gutterBottom>
                      Active Techniques:
                    </Typography>
                    <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 0.5, mt: 0.5 }}>
                      {activeTechniques.map((technique) => (
                        <Chip
                          key={technique}
                          label={TECHNIQUE_NAMES[technique] || technique}
                          size="small"
                          color="primary"
                          variant="outlined"
                        />
                      ))}
                    </Box>
                  </Box>
                )}
                <Typography variant="caption" color="text.secondary" display="block" sx={{ mt: 1 }}>
                  {powerData ? 'No recent energy reports' : 'Waiting for first energy report...'}
                </Typography>
              </Alert>
            )}
          </>
        )}

        {/* Chart View */}
        {viewMode === 1 && (
          <Box>
            {chartData.length > 0 ? (
              <>
                {/* Current Consumption Chart */}
                <Box sx={{ mb: 4 }}>
                  <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                    Current Consumption (24h)
                  </Typography>
                  <ResponsiveContainer width="100%" height={250}>
                    <LineChart data={chartData}>
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="timestamp" />
                      <YAxis label={{ value: 'mA', angle: -90, position: 'insideLeft' }} />
                      <RechartsTooltip />
                      <Legend />
                      <Line type="monotone" dataKey="current" stroke="#1976d2" name="Current (mA)" />
                    </LineChart>
                  </ResponsiveContainer>
                </Box>

                {/* Energy Saved Chart */}
                <Box>
                  <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                    Energy Saved (24h)
                  </Typography>
                  <ResponsiveContainer width="100%" height={250}>
                    <LineChart data={chartData}>
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="timestamp" />
                      <YAxis label={{ value: 'mAh', angle: -90, position: 'insideLeft' }} />
                      <RechartsTooltip />
                      <Legend />
                      <Line type="monotone" dataKey="energySaved" stroke="#4caf50" name="Energy Saved (mAh)" />
                    </LineChart>
                  </ResponsiveContainer>
                </Box>
              </>
            ) : (
              <Alert severity="info">No historical data available for charts</Alert>
            )}
          </Box>
        )}

        {/* Table View */}
        {viewMode === 2 && (
          <Box>
            {historicalData.length > 0 ? (
              <TableContainer component={Paper} variant="outlined" sx={{ maxHeight: 500 }}>
                <Table stickyHeader size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell><strong>Timestamp</strong></TableCell>
                      <TableCell align="right"><strong>Current (mA)</strong></TableCell>
                      <TableCell align="right"><strong>Energy Saved (mAh)</strong></TableCell>
                      <TableCell align="right"><strong>Uptime</strong></TableCell>
                      <TableCell><strong>Techniques</strong></TableCell>
                      <TableCell align="center"><strong>Status</strong></TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {historicalData.map((report, index) => (
                      <TableRow key={index} hover>
                        <TableCell>{formatTimestamp(report.timestamp)}</TableCell>
                        <TableCell align="right">{report.avg_current_ma.toFixed(2)}</TableCell>
                        <TableCell align="right">{report.energy_saved_mah.toFixed(2)}</TableCell>
                        <TableCell align="right">{formatUptime(report.uptime_ms)}</TableCell>
                        <TableCell>
                          <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 0.5 }}>
                            {report.techniques_list && report.techniques_list.length > 0 ? (
                              report.techniques_list.map((tech) => (
                                <Chip 
                                  key={tech} 
                                  label={TECHNIQUE_NAMES[tech] || tech} 
                                  size="small" 
                                  variant="outlined"
                                />
                              ))
                            ) : (
                              <Typography variant="caption" color="text.secondary">None</Typography>
                            )}
                          </Box>
                        </TableCell>
                        <TableCell align="center">
                          <Chip 
                            label={report.enabled ? "Enabled" : "Disabled"} 
                            size="small" 
                            color={report.enabled ? "success" : "default"}
                          />
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </TableContainer>
            ) : (
              <Alert severity="info">No historical data available</Alert>
            )}
          </Box>
        )}

        {error && (
          <Alert severity="error" sx={{ mt: 2 }}>
            {error}
          </Alert>
        )}
      </CardContent>
    </Card>
  );
};

export default PowerWidget;
