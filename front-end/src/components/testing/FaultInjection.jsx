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
  LinearProgress,
  TablePagination,
  IconButton,
  Collapse
} from '@mui/material';
import {
  BugReport as FaultIcon,
  PlayArrow as InjectIcon,
  Clear as ClearIcon,
  Warning as WarningIcon,
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  Refresh as RefreshIcon,
  ExpandMore as ExpandMoreIcon,
  ExpandLess as ExpandLessIcon,
  Code as CodeIcon
} from '@mui/icons-material';
import { useMutation, useQuery } from '@tanstack/react-query';
import { injectFault, clearFaults, getRecoveryEvents, getAllRecoveryEvents, clearRecoveryEvents, getInjectionHistory, getOTAFaultStatus, clearOTAFaults, getNetworkFaultStatus, clearNetworkFaults } from '../../api/faults';
import { getDevices } from '../../api/devices';

const FaultInjection = () => {
  const [backend, setBackend] = useState('inverter_sim');
  const [faultType, setFaultType] = useState('EXCEPTION');
  const [deviceId, setDeviceId] = useState('');
  const [selectedRecoveryDevice, setSelectedRecoveryDevice] = useState('');
  
  // Additional parameters for specific fault types
  const [exceptionCode, setExceptionCode] = useState(2);
  const [delayMs, setDelayMs] = useState(5000);
  const [maxChunkPercent, setMaxChunkPercent] = useState(50);
  const [interruptChunk, setInterruptChunk] = useState(100);
  const [failureRate, setFailureRate] = useState(0.5);
  const [targetEndpoint, setTargetEndpoint] = useState('/power/upload');
  const [probability, setProbability] = useState(100);
  
  // Pagination for injection history
  const [injectionPage, setInjectionPage] = useState(0);
  const [injectionRowsPerPage, setInjectionRowsPerPage] = useState(10);
  
  // Pagination for recovery events
  const [recoveryPage, setRecoveryPage] = useState(0);
  const [recoveryRowsPerPage, setRecoveryRowsPerPage] = useState(10);
  
  // Expanded rows for JSON view
  const [expandedRecoveryRows, setExpandedRecoveryRows] = useState(new Set());

  // Fetch devices
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Fetch injection history from database (auto-refresh every 5s)
  const { data: injectionData, refetch: refetchInjections } = useQuery({
    queryKey: ['injectionHistory'],
    queryFn: () => getInjectionHistory(null, 100), // Fetch last 100 injections for pagination
    refetchInterval: 5000, // Auto-refresh every 5 seconds
    staleTime: 4000
  });

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

  // Fetch OTA fault injection status (auto-refresh every 2s)
  const { data: otaFaultData, refetch: refetchOTAFault } = useQuery({
    queryKey: ['otaFaultStatus'],
    queryFn: getOTAFaultStatus,
    refetchInterval: 2000, // Auto-refresh every 2 seconds
    staleTime: 1000
  });

  const otaFaultStatus = otaFaultData?.data?.fault_injection || null;

  // Fetch Network fault injection status (auto-refresh every 2s)
  const { data: networkFaultData, refetch: refetchNetworkFault } = useQuery({
    queryKey: ['networkFaultStatus'],
    queryFn: getNetworkFaultStatus,
    refetchInterval: 2000, // Auto-refresh every 2 seconds
    staleTime: 1000
  });

  const networkFaultStatus = networkFaultData?.data?.network_fault || null;
  const esp32Endpoints = networkFaultData?.data?.esp32_endpoints || [];

  // Inverter SIM fault types (from external API - Milestone 5)
  const inverterSimFaults = [
    { value: 'EXCEPTION', label: 'Modbus Exception', description: 'Modbus exception with error code', exceptionCode: true },
    { value: 'CRC_ERROR', label: 'CRC Error', description: 'Malformed CRC frames' },
    { value: 'CORRUPT', label: 'Corrupt Data', description: 'Random byte garbage' },
    { value: 'PACKET_DROP', label: 'Packet Drop', description: 'Dropped packets (no response)' },
    { value: 'DELAY', label: 'Response Delay', description: 'Add delay to responses', requiresDelay: true }
  ];

  // Local fault types - OTA (Milestone 5 - FOTA Rollback Demo)
  const otaFaults = [
    { value: 'corrupt_chunk', label: 'Corrupt Chunk', description: 'Corrupt firmware chunk data (triggers rollback)' },
    { value: 'bad_hash', label: 'Bad Hash', description: 'Incorrect SHA256 hash in manifest (triggers rollback)' },
    { value: 'bad_signature', label: 'Bad Signature', description: 'Incorrect signature in manifest (triggers rollback)' }
  ];

  // Local fault types - Network (Milestone 5)
  const networkFaults = [
    { value: 'timeout', label: 'Connection Timeout', description: 'Connection timeout (504 error)', requiresDelay: true },
    { value: 'disconnect', label: 'Connection Drop', description: 'Immediate connection failure (503)' },
    { value: 'slow', label: 'Slow Network', description: 'Add delay to responses', requiresDelay: true }
  ];

  const faultOptions = backend === 'inverter_sim' 
    ? inverterSimFaults 
    : backend === 'ota' 
    ? otaFaults 
    : networkFaults;

  // Inject fault mutation
  const injectMutation = useMutation({
    mutationFn: () => {
      // Build payload based on backend type (Milestone 5 format)
      let payload;
      
      if (backend === 'inverter_sim') {
        // Inverter SIM API payload (Section 7 format - no slaveAddress/functionCode)
        payload = {
          errorType: faultType,
          exceptionCode: faultType === 'EXCEPTION' ? exceptionCode : 0,
          delayMs: faultType === 'DELAY' ? delayMs : 0
        };
      } else if (backend === 'ota') {
        // OTA fault payload
        payload = {
          fault_type: 'ota',
          ota_fault_subtype: faultType,
          target_device: deviceId || undefined,
          parameters: {}
        };
        
        // Add parameters based on fault type
        if (faultType === 'partial_download') {
          payload.parameters.max_chunk_percent = maxChunkPercent;
        } else if (faultType === 'network_interrupt') {
          payload.parameters.interrupt_after_chunk = interruptChunk;
        } else if (faultType === 'timeout') {
          payload.parameters.delay_ms = delayMs;
        }
      } else if (backend === 'network') {
        // Network fault payload
        payload = {
          fault_type: 'network',
          network_fault_subtype: faultType,
          target_endpoint: targetEndpoint || undefined,
          probability: probability,
          parameters: {}
        };
        
        // Add parameters based on fault type
        if (faultType === 'timeout') {
          payload.parameters.timeout_ms = delayMs;
        } else if (faultType === 'slow') {
          payload.parameters.delay_ms = delayMs;
        }
      }
      
      return injectFault(payload);
    },
    onSuccess: () => {
      // Refetch injection history from database
      refetchInjections();
      // Refetch OTA fault status if OTA backend
      if (backend === 'ota') {
        refetchOTAFault();
      }
      // Refetch Network fault status if network backend
      if (backend === 'network') {
        refetchNetworkFault();
      }
    },
    onError: () => {
      // Refetch to show failed injection in database
      refetchInjections();
    }
  });

  // Clear faults mutation
  const clearMutation = useMutation({
    mutationFn: () => clearFaults(backend),
    onSuccess: () => {
      refetchInjections();
    }
  });

  // Clear OTA faults mutation
  const clearOTAMutation = useMutation({
    mutationFn: clearOTAFaults,
    onSuccess: () => {
      refetchOTAFault();
      refetchInjections();
    }
  });

  // Clear Network faults mutation
  const clearNetworkMutation = useMutation({
    mutationFn: clearNetworkFaults,
    onSuccess: () => {
      refetchNetworkFault();
      refetchInjections();
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

  const getCurrentFault = () => {
    return faultOptions.find(f => f.value === faultType);
  };

  // Pagination handlers for injection history
  const handleInjectionPageChange = (event, newPage) => {
    setInjectionPage(newPage);
  };

  const handleInjectionRowsPerPageChange = (event) => {
    setInjectionRowsPerPage(parseInt(event.target.value, 10));
    setInjectionPage(0);
  };

  // Pagination handlers for recovery events
  const handleRecoveryPageChange = (event, newPage) => {
    setRecoveryPage(newPage);
  };

  const handleRecoveryRowsPerPageChange = (event) => {
    setRecoveryRowsPerPage(parseInt(event.target.value, 10));
    setRecoveryPage(0);
  };

  // Paginated data
  const injections = injectionData?.data?.injections || [];
  const paginatedInjections = injections.slice(
    injectionPage * injectionRowsPerPage,
    injectionPage * injectionRowsPerPage + injectionRowsPerPage
  );

  const recoveryEvents = recoveryData?.data?.events || [];
  const paginatedRecoveryEvents = recoveryEvents.slice(
    recoveryPage * recoveryRowsPerPage,
    recoveryPage * recoveryRowsPerPage + recoveryRowsPerPage
  );
  
  // Get statistics (handles both 'statistics' for device-specific and 'global_statistics' for all devices)
  const recoveryStats = recoveryData?.data?.statistics || recoveryData?.data?.global_statistics || {
    total: 0,
    successful: 0,
    failed: 0,
    success_rate: 0
  };
  
  // Toggle row expansion for JSON view
  const toggleRecoveryRowExpand = (index) => {
    setExpandedRecoveryRows(prev => {
      const newSet = new Set(prev);
      if (newSet.has(index)) {
        newSet.delete(index);
      } else {
        newSet.add(index);
      }
      return newSet;
    });
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
                // Reset fault type based on backend
                if (e.target.value === 'inverter_sim') {
                  setFaultType('EXCEPTION');
                } else if (e.target.value === 'ota') {
                  setFaultType('bad_hash'); // Default to Bad Hash for FOTA demo
                } else {
                  setFaultType('timeout');
                }
              }}
            >
              <MenuItem value="inverter_sim">
                Inverter SIM API (Modbus Faults - External)
              </MenuItem>
              <MenuItem value="ota">
                OTA Faults (Firmware Update - Local)
              </MenuItem>
              <MenuItem value="network">
                Network Faults (Connectivity - Local)
              </MenuItem>
            </Select>
          </FormControl>
          <Alert severity="info" sx={{ mt: 1 }}>
            {backend === 'inverter_sim' ? (
              <>
                <strong>Inverter SIM:</strong> Injects Modbus faults (CRC errors, exceptions, packet drops) for ESP32 testing
              </>
            ) : backend === 'ota' ? (
              <>
                <strong>OTA Faults:</strong> Simulates firmware update failures (corrupted chunks, hash mismatches, partial downloads)
              </>
            ) : (
              <>
                <strong>Network Faults:</strong> Simulates network issues (timeouts, disconnects, slow connections)
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

        {/* Fault-Specific Parameters */}
        {getCurrentFault()?.exceptionCode && (
          <Box sx={{ mb: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              Exception Code
            </Typography>
            <FormControl fullWidth sx={{ mt: 1 }}>
              <InputLabel>Modbus Exception Code</InputLabel>
              <Select
                value={exceptionCode}
                label="Modbus Exception Code"
                onChange={(e) => setExceptionCode(e.target.value)}
              >
                <MenuItem value={1}>01 - Illegal Function</MenuItem>
                <MenuItem value={2}>02 - Illegal Data Address</MenuItem>
                <MenuItem value={3}>03 - Illegal Data Value</MenuItem>
                <MenuItem value={4}>04 - Slave Device Failure</MenuItem>
                <MenuItem value={6}>06 - Slave Device Busy</MenuItem>
              </Select>
            </FormControl>
          </Box>
        )}

        {getCurrentFault()?.requiresDelay && (
          <Box sx={{ mb: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              Delay (milliseconds)
            </Typography>
            <TextField
              fullWidth
              type="number"
              value={delayMs}
              onChange={(e) => setDelayMs(parseInt(e.target.value) || 0)}
              helperText="How long to delay response (ms)"
              sx={{ mt: 1 }}
            />
          </Box>
        )}

        {getCurrentFault()?.requiresPercent && (
          <Box sx={{ mb: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              Download Percentage Limit
            </Typography>
            <TextField
              fullWidth
              type="number"
              value={maxChunkPercent}
              onChange={(e) => setMaxChunkPercent(Math.min(100, Math.max(0, parseInt(e.target.value) || 0)))}
              helperText="Stop download at this percentage (0-100%)"
              inputProps={{ min: 0, max: 100 }}
              sx={{ mt: 1 }}
            />
          </Box>
        )}

        {getCurrentFault()?.requiresChunk && (
          <Box sx={{ mb: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              Interrupt After Chunk
            </Typography>
            <TextField
              fullWidth
              type="number"
              value={interruptChunk}
              onChange={(e) => setInterruptChunk(parseInt(e.target.value) || 0)}
              helperText="Stop after downloading this chunk number"
              sx={{ mt: 1 }}
            />
          </Box>
        )}

        {getCurrentFault()?.requiresFailureRate && (
          <Box sx={{ mb: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              Failure Rate (0.0 - 1.0)
            </Typography>
            <TextField
              fullWidth
              type="number"
              value={failureRate}
              onChange={(e) => setFailureRate(Math.min(1.0, Math.max(0.0, parseFloat(e.target.value) || 0)))}
              helperText="Probability of intermittent failure (0.0 = never, 1.0 = always)"
              inputProps={{ min: 0, max: 1, step: 0.1 }}
              sx={{ mt: 1 }}
            />
          </Box>
        )}

        {backend === 'network' && (
          <>
            <Box sx={{ mb: 3 }}>
              <Typography variant="subtitle2" gutterBottom>
                Target Endpoint (Optional)
              </Typography>
              <TextField
                fullWidth
                value={targetEndpoint}
                onChange={(e) => setTargetEndpoint(e.target.value)}
                placeholder="/power/upload"
                helperText="Leave empty to target all endpoints"
                sx={{ mt: 1 }}
              />
            </Box>
            <Box sx={{ mb: 3 }}>
              <Typography variant="subtitle2" gutterBottom>
                Probability (%)
              </Typography>
              <TextField
                fullWidth
                type="number"
                value={probability}
                onChange={(e) => setProbability(Math.min(100, Math.max(0, parseInt(e.target.value) || 0)))}
                helperText="Probability of fault occurring (0-100%)"
                inputProps={{ min: 0, max: 100 }}
                sx={{ mt: 1 }}
              />
            </Box>
          </>
        )}

        {/* Device Selection (Optional) */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            {backend === 'ota' ? '3. Select Target Device' : '3. Select Target Device (Optional)'}
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Device</InputLabel>
            <Select
              value={deviceId}
              label="Device"
              onChange={(e) => setDeviceId(e.target.value)}
            >
              <MenuItem value="">{backend === 'ota' ? 'All Devices' : 'All Devices'}</MenuItem>
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

        {/* OTA Fault Status Card */}
        {otaFaultStatus && (
          <Card 
            variant="outlined" 
            sx={{ 
              mb: 3, 
              bgcolor: otaFaultStatus.enabled ? 'warning.50' : 'grey.50',
              borderColor: otaFaultStatus.enabled ? 'warning.main' : 'divider'
            }}
          >
            <CardContent>
              <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2 }}>
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                  <WarningIcon color={otaFaultStatus.enabled ? 'warning' : 'disabled'} />
                  <Typography variant="subtitle1" fontWeight="bold">
                    OTA Fault Injection Status
                  </Typography>
                </Box>
                <Chip 
                  label={otaFaultStatus.enabled ? 'ACTIVE' : 'DISABLED'} 
                  color={otaFaultStatus.enabled ? 'warning' : 'default'}
                  size="small"
                />
              </Box>
              
              {otaFaultStatus.enabled ? (
                <>
                  <Grid container spacing={2} sx={{ mb: 2 }}>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Fault Type</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {otaFaultStatus.fault_type}
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Target Device</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {otaFaultStatus.target_device || 'All Devices'}
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Target Chunk</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {otaFaultStatus.target_chunk !== null ? otaFaultStatus.target_chunk : 'All Chunks'}
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Faults Injected</Typography>
                      <Typography variant="body2" fontWeight="bold" color="error.main">
                        {otaFaultStatus.fault_count}
                      </Typography>
                    </Grid>
                  </Grid>
                  
                  {otaFaultStatus.chunks_corrupted && otaFaultStatus.chunks_corrupted.length > 0 && (
                    <Alert severity="error" sx={{ mb: 2 }}>
                      Corrupted chunks: {otaFaultStatus.chunks_corrupted.join(', ')}
                    </Alert>
                  )}
                  
                  <Button
                    variant="contained"
                    color="warning"
                    onClick={() => clearOTAMutation.mutate()}
                    disabled={clearOTAMutation.isPending}
                    startIcon={clearOTAMutation.isPending ? <CircularProgress size={16} /> : <ClearIcon />}
                    fullWidth
                  >
                    {clearOTAMutation.isPending ? 'Disabling...' : 'Disable OTA Fault Injection'}
                  </Button>
                </>
              ) : (
                <Alert severity="info">
                  No OTA fault injection active. Select "OTA Faults" backend and inject a fault to enable.
                </Alert>
              )}
            </CardContent>
          </Card>
        )}

        {/* Network Fault Status Card */}
        {networkFaultStatus && (
          <Card 
            variant="outlined" 
            sx={{ 
              mb: 3, 
              bgcolor: networkFaultStatus.enabled ? 'error.50' : 'grey.50',
              borderColor: networkFaultStatus.enabled ? 'error.main' : 'divider'
            }}
          >
            <CardContent>
              <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2 }}>
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                  <ErrorIcon color={networkFaultStatus.enabled ? 'error' : 'disabled'} />
                  <Typography variant="subtitle1" fontWeight="bold">
                    Network Fault Injection Status
                  </Typography>
                </Box>
                <Chip 
                  label={networkFaultStatus.enabled ? 'ACTIVE' : 'DISABLED'} 
                  color={networkFaultStatus.enabled ? 'error' : 'default'}
                  size="small"
                />
              </Box>
              
              {networkFaultStatus.enabled ? (
                <>
                  <Grid container spacing={2} sx={{ mb: 2 }}>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Fault Type</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {networkFaultStatus.fault_type}
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Target Endpoint</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {networkFaultStatus.target_endpoint || 'All ESP32 Endpoints'}
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Probability</Typography>
                      <Typography variant="body2" fontWeight="bold">
                        {networkFaultStatus.probability}%
                      </Typography>
                    </Grid>
                    <Grid item xs={6} sm={3}>
                      <Typography variant="caption" color="text.secondary">Faults Injected</Typography>
                      <Typography variant="body2" fontWeight="bold" color="error.main">
                        {networkFaultStatus.faults_injected}
                      </Typography>
                    </Grid>
                  </Grid>
                  
                  {networkFaultStatus.parameters && Object.keys(networkFaultStatus.parameters).length > 0 && (
                    <Alert severity="info" sx={{ mb: 2 }}>
                      Parameters: {Object.entries(networkFaultStatus.parameters).map(([k, v]) => `${k}=${v}`).join(', ')}
                    </Alert>
                  )}

                  <Alert severity="warning" sx={{ mb: 2 }}>
                    <strong>Affected Endpoints:</strong> {esp32Endpoints.join(', ')}
                  </Alert>
                  
                  <Button
                    variant="contained"
                    color="error"
                    onClick={() => clearNetworkMutation.mutate()}
                    disabled={clearNetworkMutation.isPending}
                    startIcon={clearNetworkMutation.isPending ? <CircularProgress size={16} /> : <ClearIcon />}
                    fullWidth
                  >
                    {clearNetworkMutation.isPending ? 'Disabling...' : 'Disable Network Fault Injection'}
                  </Button>
                </>
              ) : (
                <Alert severity="info">
                  No network fault injection active. Select "Network Faults" backend and inject a fault to enable.
                  <br />
                  <strong>Note:</strong> Network faults only affect ESP32 endpoints: {esp32Endpoints.join(', ')}
                </Alert>
              )}
            </CardContent>
          </Card>
        )}

        {/* Fault Injection History from Database */}
        {injections.length > 0 && (
          <Box>
            <Divider sx={{ my: 3 }} />
            <Box sx={{ mb: 2 }}>
              <Typography variant="h6" gutterBottom>
                Fault Injection History (Database)
              </Typography>
              <Alert severity="info">
                Total: {injections.length} injections • Auto-refreshes every 5 seconds
              </Alert>
            </Box>

            <List sx={{ bgcolor: 'grey.50', borderRadius: 1 }}>
              {paginatedInjections.map((injection, index) => (
                <React.Fragment key={injection.id}>
                  <ListItem>
                    <ListItemText
                      primary={
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          <Chip
                            label={injection.success ? 'Success' : 'Failed'}
                            color={injection.success ? 'success' : 'error'}
                            size="small"
                          />
                          <Typography variant="body2" fontWeight="bold">
                            {injection.fault_type || injection.error_type}
                          </Typography>
                          <Typography variant="body2" color="text.secondary">
                            on {injection.backend}
                          </Typography>
                          {injection.device_id && (
                            <Typography variant="body2" color="text.secondary">
                              • Device: {injection.device_id}
                            </Typography>
                          )}
                          {injection.exception_code && (
                            <Chip
                              label={`Exception: 0x${injection.exception_code.toString(16).toUpperCase()}`}
                              size="small"
                              variant="outlined"
                            />
                          )}
                          {injection.delay_ms && (
                            <Chip
                              label={`${injection.delay_ms}ms delay`}
                              size="small"
                              variant="outlined"
                            />
                          )}
                        </Box>
                      }
                      secondary={
                        <Box sx={{ mt: 0.5 }}>
                          {!injection.success && injection.error_msg && (
                            <Typography variant="body2" color="error">
                              Error: {injection.error_msg}
                            </Typography>
                          )}
                          <Typography variant="caption" color="text.secondary">
                            {new Date(injection.created_at).toLocaleString()}
                          </Typography>
                        </Box>
                      }
                    />
                  </ListItem>
                  {index < paginatedInjections.length - 1 && <Divider />}
                </React.Fragment>
              ))}
            </List>

            {/* Pagination Controls */}
            <TablePagination
              component="div"
              count={injections.length}
              page={injectionPage}
              onPageChange={handleInjectionPageChange}
              rowsPerPage={injectionRowsPerPage}
              onRowsPerPageChange={handleInjectionRowsPerPageChange}
              rowsPerPageOptions={[5, 10, 25, 50]}
              labelRowsPerPage="Items per page:"
            />
          </Box>
        )}

        {/* Fault Recovery Events Viewer (Milestone 5) */}
        <Box sx={{ mt: 4 }}>
          <Divider sx={{ mb: 3 }} />
          <Box sx={{ mb: 2 }}>
            <Typography variant="h6" gutterBottom>
              Fault Recovery Events (Milestone 5)
            </Typography>
            <Alert severity="info" sx={{ mb: 2 }}>
              Auto-refreshes every 3 seconds
            </Alert>
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
                      {recoveryStats.total}
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
                      {recoveryStats.successful}
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
                      {recoveryStats.failed}
                    </Typography>
                  </CardContent>
                </Card>
              </Grid>
              {recoveryStats.success_rate !== undefined && (
                <Grid item xs={12}>
                  <Card variant="outlined">
                    <CardContent>
                      <Typography variant="body2" color="text.secondary" gutterBottom>
                        Success Rate
                      </Typography>
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
                        <LinearProgress
                          variant="determinate"
                          value={recoveryStats.success_rate}
                          sx={{ flexGrow: 1, height: 10, borderRadius: 1 }}
                          color={recoveryStats.success_rate >= 80 ? 'success' : 'warning'}
                        />
                        <Typography variant="h6" fontWeight="bold">
                          {recoveryStats.success_rate.toFixed(1)}%
                        </Typography>
                      </Box>
                    </CardContent>
                  </Card>
                </Grid>
              )}
            </Grid>
          )}

          {/* Recovery Events List */}
          {recoveryEvents.length > 0 ? (
            <Box>
              <Alert severity="info" sx={{ mb: 2 }}>
                Total: {recoveryEvents.length} recovery events
              </Alert>
              
              <List sx={{ bgcolor: 'grey.50', borderRadius: 1 }}>
                {paginatedRecoveryEvents.map((event, index) => (
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
                            <Box sx={{ flexGrow: 1 }} />
                            <IconButton
                              size="small"
                              onClick={() => toggleRecoveryRowExpand(index)}
                              sx={{ ml: 'auto' }}
                            >
                              {expandedRecoveryRows.has(index) ? <ExpandLessIcon /> : <ExpandMoreIcon />}
                            </IconButton>
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
                            
                            {/* Expandable JSON View */}
                            <Collapse in={expandedRecoveryRows.has(index)} timeout="auto" unmountOnExit>
                              <Box sx={{ mt: 2, p: 2, bgcolor: 'grey.100', borderRadius: 1 }}>
                                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 1 }}>
                                  <CodeIcon fontSize="small" />
                                  <Typography variant="subtitle2" fontWeight="bold">
                                    Recovery Event JSON
                                  </Typography>
                                </Box>
                                <Box sx={{ 
                                  p: 1.5, 
                                  bgcolor: 'background.paper', 
                                  borderRadius: 1,
                                  border: '1px solid',
                                  borderColor: 'divider'
                                }}>
                                  <Typography 
                                    variant="body2" 
                                    component="pre"
                                    sx={{ 
                                      fontFamily: 'monospace',
                                      fontSize: '0.75rem',
                                      margin: 0,
                                      whiteSpace: 'pre-wrap',
                                      wordBreak: 'break-word'
                                    }}
                                  >
                                    {JSON.stringify({
                                      device_id: event.device_id,
                                      timestamp: event.timestamp,
                                      fault_type: event.fault_type,
                                      recovery_action: event.recovery_action,
                                      success: event.success,
                                      details: event.details,
                                      retry_count: event.retry_count,
                                      received_at: event.received_at
                                    }, null, 2)}
                                  </Typography>
                                </Box>
                              </Box>
                            </Collapse>
                          </Box>
                        }
                      />
                    </ListItem>
                    {index < paginatedRecoveryEvents.length - 1 && <Divider />}
                  </React.Fragment>
                ))}
              </List>

              {/* Pagination Controls */}
              <TablePagination
                component="div"
                count={recoveryEvents.length}
                page={recoveryPage}
                onPageChange={handleRecoveryPageChange}
                rowsPerPage={recoveryRowsPerPage}
                onRowsPerPageChange={handleRecoveryRowsPerPageChange}
                rowsPerPageOptions={[5, 10, 25, 50]}
                labelRowsPerPage="Items per page:"
              />
            </Box>
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
                    OTA Faults
                  </Typography>
                  {otaFaults.map((fault) => (
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
                    Network Faults
                  </Typography>
                  {networkFaults.map((fault) => (
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
```
          </Grid>
        </Box>
      </Paper>
    </Box>
  );
};

export default FaultInjection;
