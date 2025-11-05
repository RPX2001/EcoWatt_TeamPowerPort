/**
 * FaultInjection Component
 * 
 * Tool for testing system behavior with fault injection
 * Features:
 * - Multiple fault types (Inverter SIM and Local)
 * - Device selection
 * - Fault injection execution
 * - Result log viewer
 * - Fault history
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Button,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  TextField,
  Alert,
  CircularProgress,
  Divider,
  Stack,
  Chip,
  List,
  ListItem,
  ListItemText,
  Grid,
  Card,
  CardContent,
  LinearProgress
} from '@mui/material';
import {
  BugReport as FaultIcon,
  PlayArrow as InjectIcon,
  Clear as ClearIcon,
  Warning as WarningIcon,
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  Refresh as RefreshIcon
} from '@mui/icons-material';
import { useMutation, useQuery } from '@tanstack/react-query';
import { injectFault, clearFaults, getRecoveryEvents, clearRecoveryEvents } from '../../api/faults';
import { getDevices } from '../../api/devices';

const FaultInjection = () => {
  const [backend, setBackend] = useState('inverter_sim');
  const [faultType, setFaultType] = useState('EXCEPTION');
  const [deviceId, setDeviceId] = useState('');
  const [faultHistory, setFaultHistory] = useState([]);
  const [selectedRecoveryDevice, setSelectedRecoveryDevice] = useState('');

  // Fetch devices
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Fetch recovery events for selected device (auto-refresh every 3s)
  const { data: recoveryData, refetch: refetchRecovery } = useQuery({
    queryKey: ['recoveryEvents', selectedRecoveryDevice],
    queryFn: () => selectedRecoveryDevice 
      ? getRecoveryEvents(selectedRecoveryDevice) 
      : getAllRecoveryEvents(),
    enabled: !!selectedRecoveryDevice || selectedRecoveryDevice === '',
    refetchInterval: 3000, // Auto-refresh every 3 seconds
    staleTime: 2000
  });

  // Inverter SIM fault types (from external API)
  const inverterSimFaults = [
    { value: 'EXCEPTION', label: 'Exception Error', description: 'Trigger an exception in data processing' },
    { value: 'CRC_ERROR', label: 'CRC Error', description: 'Corrupt data with CRC mismatch' },
    { value: 'CORRUPT', label: 'Data Corruption', description: 'Send corrupted/malformed data' },
    { value: 'PACKET_DROP', label: 'Packet Drop', description: 'Drop packets to simulate loss' },
    { value: 'DELAY', label: 'Network Delay', description: 'Add artificial delay to responses' }
  ];

  // Local fault types (internal simulation)
  const localFaults = [
    { value: 'network_timeout', label: 'Network Timeout', description: 'Simulate network timeout' },
    { value: 'mqtt_disconnect', label: 'MQTT Disconnect', description: 'Simulate MQTT connection loss' },
    { value: 'command_failure', label: 'Command Failure', description: 'Simulate command execution failure' },
    { value: 'ota_failure', label: 'OTA Failure', description: 'Simulate OTA update failure' },
    { value: 'memory_error', label: 'Memory Error', description: 'Simulate memory allocation error' }
  ];

  const faultOptions = backend === 'inverter_sim' ? inverterSimFaults : localFaults;

  // Inject fault mutation
  const injectMutation = useMutation({
    mutationFn: () => {
      // Build payload based on backend type
      let payload;
      
      if (backend === 'inverter_sim') {
        payload = {
          slaveAddress: 1,
          functionCode: 3,
          errorType: faultType
        };
        
        // Add optional fields based on fault type
        if (faultType === 'EXCEPTION') {
          payload.exceptionCode = 2; // Illegal Data Address
        } else if (faultType === 'DELAY') {
          payload.delayMs = 2000; // 2 second delay
        }
      } else {
        // Local fault
        payload = {
          fault_type: faultType,
          target: 'test',
          duration: 60
        };
      }
      
      return injectFault(payload);
    },
    onSuccess: (response) => {
      const newFault = {
        timestamp: new Date().toISOString(),
        backend,
        type: faultType,
        device: deviceId || 'All',
        status: 'success',
        message: response.data.message || 'Fault injected successfully'
      };
      setFaultHistory(prev => [newFault, ...prev].slice(0, 10));
    },
    onError: (error) => {
      const newFault = {
        timestamp: new Date().toISOString(),
        backend,
        type: faultType,
        device: deviceId || 'All',
        status: 'error',
        message: error.response?.data?.error || 'Failed to inject fault'
      };
      setFaultHistory(prev => [newFault, ...prev].slice(0, 10));
    }
  });

  // Clear faults mutation
  const clearMutation = useMutation({
    mutationFn: () => clearFaults(backend),
    onSuccess: () => {
      const newEntry = {
        timestamp: new Date().toISOString(),
        backend,
        type: 'CLEAR',
        device: 'All',
        status: 'success',
        message: 'Faults cleared successfully'
      };
      setFaultHistory(prev => [newEntry, ...prev].slice(0, 10));
    }
  });

  // Clear recovery events mutation
  const clearRecoveryMutation = useMutation({
    mutationFn: () => clearRecoveryEvents(selectedRecoveryDevice || null),
    onSuccess: () => {
      refetchRecovery();
    }
  });

  const handleInject = () => {
    injectMutation.mutate();
  };

  const handleClear = () => {
    clearMutation.mutate();
  };

  const handleClearHistory = () => {
    setFaultHistory([]);
  };

  const getCurrentFault = () => {
    return faultOptions.find(f => f.value === faultType);
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          Fault Injection Testing
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Test system behavior and error handling with controlled fault injection
        </Typography>

        <Divider sx={{ my: 3 }} />

        {/* Backend Selection */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            1. Select Fault Backend
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Backend</InputLabel>
            <Select
              value={backend}
              label="Backend"
              onChange={(e) => {
                setBackend(e.target.value);
                setFaultType(e.target.value === 'inverter_sim' ? 'EXCEPTION' : 'network_timeout');
              }}
            >
              <MenuItem value="inverter_sim">
                Inverter SIM API (External - Real hardware simulation)
              </MenuItem>
              <MenuItem value="local">
                Local Backend (Internal - Server simulation)
              </MenuItem>
            </Select>
          </FormControl>
          <Alert severity="info" sx={{ mt: 1 }}>
            {backend === 'inverter_sim' ? (
              <>
                <strong>Inverter SIM API:</strong> Uses external API (20.15.114.131:8080) to inject faults into actual inverter hardware simulation
              </>
            ) : (
              <>
                <strong>Local Backend:</strong> Simulates faults internally in the Flask server for testing without external dependencies
              </>
            )}
          </Alert>
        </Box>

        {/* Fault Type Selection */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            2. Select Fault Type
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Fault Type</InputLabel>
            <Select
              value={faultType}
              label="Fault Type"
              onChange={(e) => setFaultType(e.target.value)}
            >
              {faultOptions.map((fault) => (
                <MenuItem key={fault.value} value={fault.value}>
                  {fault.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
          {getCurrentFault() && (
            <Alert severity="warning" icon={<WarningIcon />} sx={{ mt: 1 }}>
              <strong>{getCurrentFault().label}:</strong> {getCurrentFault().description}
            </Alert>
          )}
        </Box>

        {/* Device Selection (Optional) */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            3. Select Target Device (Optional)
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Device</InputLabel>
            <Select
              value={deviceId}
              label="Device"
              onChange={(e) => setDeviceId(e.target.value)}
            >
              <MenuItem value="">All Devices</MenuItem>
              {devices.map((device) => (
                <MenuItem key={device.device_id} value={device.device_id}>
                  {device.device_name || device.device_id}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        {/* Action Buttons */}
        <Stack direction="row" spacing={2} sx={{ mb: 3 }}>
          <Button
            variant="contained"
            color="error"
            onClick={handleInject}
            disabled={injectMutation.isPending}
            startIcon={injectMutation.isPending ? <CircularProgress size={20} /> : <InjectIcon />}
            fullWidth
          >
            {injectMutation.isPending ? 'Injecting Fault...' : 'Inject Fault'}
          </Button>
          <Button
            variant="outlined"
            onClick={handleClear}
            disabled={clearMutation.isPending}
            startIcon={<ClearIcon />}
          >
            Clear Faults
          </Button>
        </Stack>

        {/* Fault History */}
        {faultHistory.length > 0 && (
          <Box>
            <Divider sx={{ my: 3 }} />
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
              <Typography variant="h6">
                Fault Injection History
              </Typography>
              <Button
                size="small"
                onClick={handleClearHistory}
                startIcon={<ClearIcon />}
              >
                Clear History
              </Button>
            </Box>

            <List sx={{ bgcolor: 'grey.50', borderRadius: 1 }}>
              {faultHistory.map((entry, index) => (
                <React.Fragment key={index}>
                  <ListItem>
                    <ListItemText
                      primary={
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          <Chip
                            label={entry.status}
                            color={entry.status === 'success' ? 'success' : 'error'}
                            size="small"
                          />
                          <Typography variant="body2" fontWeight="bold">
                            {entry.type}
                          </Typography>
                          <Typography variant="body2" color="text.secondary">
                            on {entry.backend}
                          </Typography>
                          <Typography variant="body2" color="text.secondary">
                            • Device: {entry.device}
                          </Typography>
                        </Box>
                      }
                      secondary={
                        <Box sx={{ mt: 0.5 }}>
                          <Typography variant="body2">
                            {entry.message}
                          </Typography>
                          <Typography variant="caption" color="text.secondary">
                            {new Date(entry.timestamp).toLocaleString()}
                          </Typography>
                        </Box>
                      }
                    />
                  </ListItem>
                  {index < faultHistory.length - 1 && <Divider />}
                </React.Fragment>
              ))}
            </List>
          </Box>
        )}

        {/* Fault Recovery Events Viewer (Milestone 5) */}
        <Box sx={{ mt: 4 }}>
          <Divider sx={{ mb: 3 }} />
          <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
            <Typography variant="h6">
              Fault Recovery Events (Milestone 5)
            </Typography>
            <Stack direction="row" spacing={1}>
              <Button
                size="small"
                onClick={() => refetchRecovery()}
                startIcon={<RefreshIcon />}
              >
                Refresh
              </Button>
              <Button
                size="small"
                onClick={() => clearRecoveryMutation.mutate()}
                disabled={clearRecoveryMutation.isPending}
                startIcon={<ClearIcon />}
                color="error"
              >
                Clear Events
              </Button>
            </Stack>
          </Box>

          {/* Device Selector for Recovery Events */}
          <FormControl fullWidth sx={{ mb: 2 }}>
            <InputLabel>View Recovery Events For</InputLabel>
            <Select
              value={selectedRecoveryDevice}
              label="View Recovery Events For"
              onChange={(e) => setSelectedRecoveryDevice(e.target.value)}
            >
              <MenuItem value="">All Devices (Global Statistics)</MenuItem>
              {devices.map((device) => (
                <MenuItem key={device.device_id} value={device.device_id}>
                  {device.device_name || device.device_id}
                </MenuItem>
              ))}
            </Select>
          </FormControl>

          {/* Statistics Cards */}
          {recoveryData?.data && (
            <Grid container spacing={2} sx={{ mb: 3 }}>
              <Grid item xs={12} sm={4}>
                <Card variant="outlined">
                  <CardContent>
                    <Typography variant="body2" color="text.secondary" gutterBottom>
                      Total Recoveries
                    </Typography>
                    <Typography variant="h4">
                      {recoveryData.data.statistics?.total_recoveries || 0}
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              <Grid item xs={12} sm={4}>
                <Card variant="outlined" sx={{ bgcolor: 'success.50' }}>
                  <CardContent>
                    <Typography variant="body2" color="text.secondary" gutterBottom>
                      Successful
                    </Typography>
                    <Typography variant="h4" color="success.main">
                      {recoveryData.data.statistics?.successful_recoveries || 0}
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              <Grid item xs={12} sm={4}>
                <Card variant="outlined" sx={{ bgcolor: 'error.50' }}>
                  <CardContent>
                    <Typography variant="body2" color="text.secondary" gutterBottom>
                      Failed
                    </Typography>
                    <Typography variant="h4" color="error.main">
                      {recoveryData.data.statistics?.failed_recoveries || 0}
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              {recoveryData.data.statistics?.success_rate !== undefined && (
                <Grid item xs={12}>
                  <Card variant="outlined">
                    <CardContent>
                      <Typography variant="body2" color="text.secondary" gutterBottom>
                        Success Rate
                      </Typography>
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
                        <LinearProgress
                          variant="determinate"
                          value={recoveryData.data.statistics.success_rate}
                          sx={{ flexGrow: 1, height: 10, borderRadius: 1 }}
                          color={recoveryData.data.statistics.success_rate >= 80 ? 'success' : 'warning'}
                        />
                        <Typography variant="h6" fontWeight="bold">
                          {recoveryData.data.statistics.success_rate.toFixed(1)}%
                        </Typography>
                      </Box>
                    </CardContent>
                  </Card>
                </Grid>
              )}
            </Grid>
          )}

          {/* Recovery Events List */}
          {recoveryData?.data?.events && recoveryData.data.events.length > 0 ? (
            <List sx={{ bgcolor: 'grey.50', borderRadius: 1 }}>
              {recoveryData.data.events.map((event, index) => (
                <React.Fragment key={index}>
                  <ListItem>
                    <ListItemText
                      primary={
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 0.5 }}>
                          {event.success ? (
                            <SuccessIcon color="success" fontSize="small" />
                          ) : (
                            <ErrorIcon color="error" fontSize="small" />
                          )}
                          <Chip
                            label={event.success ? 'Recovered' : 'Failed'}
                            color={event.success ? 'success' : 'error'}
                            size="small"
                          />
                          <Typography variant="body2" fontWeight="bold">
                            {event.fault_type}
                          </Typography>
                          <Typography variant="body2" color="text.secondary">
                            → {event.recovery_action}
                          </Typography>
                          {event.retry_count > 0 && (
                            <Chip
                              label={`${event.retry_count} ${event.retry_count === 1 ? 'retry' : 'retries'}`}
                              size="small"
                              variant="outlined"
                            />
                          )}
                        </Box>
                      }
                      secondary={
                        <Box sx={{ mt: 0.5 }}>
                          <Typography variant="body2">
                            Device: <strong>{event.device_id}</strong>
                          </Typography>
                          {event.details && (
                            <Typography variant="body2" sx={{ mt: 0.5 }}>
                              {event.details}
                            </Typography>
                          )}
                          <Typography variant="caption" color="text.secondary">
                            {new Date(event.timestamp * 1000).toLocaleString()}
                          </Typography>
                        </Box>
                      }
                    />
                  </ListItem>
                  {index < recoveryData.data.events.length - 1 && <Divider />}
                </React.Fragment>
              ))}
            </List>
          ) : (
            <Alert severity="info">
              No recovery events yet. Inject faults to see recovery behavior.
            </Alert>
          )}
        </Box>

        {/* Available Fault Types Reference */}
        <Box sx={{ mt: 4 }}>
          <Divider sx={{ mb: 3 }} />
          <Typography variant="h6" gutterBottom>
            Available Fault Types
          </Typography>
          
          <Grid container spacing={2}>
            <Grid item xs={12} md={6}>
              <Card variant="outlined">
                <CardContent>
                  <Typography variant="subtitle1" gutterBottom fontWeight="bold">
                    Inverter SIM API Faults
                  </Typography>
                  {inverterSimFaults.map((fault) => (
                    <Box key={fault.value} sx={{ mb: 1 }}>
                      <Typography variant="body2" fontWeight="medium">
                        • {fault.label}
                      </Typography>
                      <Typography variant="caption" color="text.secondary" sx={{ ml: 2 }}>
                        {fault.description}
                      </Typography>
                    </Box>
                  ))}
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={12} md={6}>
              <Card variant="outlined">
                <CardContent>
                  <Typography variant="subtitle1" gutterBottom fontWeight="bold">
                    Local Backend Faults
                  </Typography>
                  {localFaults.map((fault) => (
                    <Box key={fault.value} sx={{ mb: 1 }}>
                      <Typography variant="body2" fontWeight="medium">
                        • {fault.label}
                      </Typography>
                      <Typography variant="caption" color="text.secondary" sx={{ ml: 2 }}>
                        {fault.description}
                      </Typography>
                    </Box>
                  ))}
                </CardContent>
              </Card>
            </Grid>
          </Grid>
        </Box>
      </Paper>
    </Box>
  );
};

export default FaultInjection;
