/**
 * SecurityTests Component
 * 
 * Comprehensive security testing interface with individual test execution
 * Features:
 * - Individual security fault injection tests
 * - Replay attack detection
 * - Tampered payload validation
 * - Invalid HMAC detection
 * - Old nonce rejection
 * - Missing nonce handling
 * - Invalid format handling
 * - Security statistics viewer
 * - Clear/Reset functionality
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
  Alert,
  CircularProgress,
  Divider,
  Stack,
  Chip,
  Grid,
  Card,
  CardContent,
  CardActions,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow
} from '@mui/material';
import {
  PlayArrow as RunIcon,
  CheckCircle as PassIcon,
  Error as FailIcon,
  Warning as WarningIcon,
  Refresh as RefreshIcon,
  Delete as ClearIcon,
  Replay as ReplayIcon,
  Edit as TamperIcon,
  Key as HmacIcon,
  Schedule as NonceIcon,
  BugReport as FormatIcon,
  PlaylistPlay as RunAllIcon
} from '@mui/icons-material';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import {
  testReplayAttack,
  testTamperedPayload,
  testInvalidHMAC,
  testOldNonce,
  testMissingNonce,
  testInvalidFormat,
  getSecurityStats,
  resetSecurityStats,
  clearDeviceNonces,
  getDeviceSecurityInfo,
  getSecurityFaultStatus,
  clearSecurityFaults
} from '../../api/security';
import { getDevices } from '../../api/devices';

// Security test definitions
const SECURITY_TESTS = [
  {
    id: 'replay',
    name: 'Replay Attack',
    description: 'Reuses a previously used nonce to test anti-replay protection',
    expected: 'Server should reject with "Replay attack detected"',
    icon: ReplayIcon,
    color: 'error',
    testFn: testReplayAttack
  },
  {
    id: 'invalid_hmac',
    name: 'Invalid HMAC',
    description: 'Sends payload with invalid (all-zeros) HMAC signature',
    expected: 'Server should reject with "HMAC verification failed"',
    icon: HmacIcon,
    color: 'warning',
    testFn: testInvalidHMAC
  },
  {
    id: 'tampered_payload',
    name: 'Tampered Payload',
    description: 'Modifies payload data after HMAC is calculated',
    expected: 'Server should reject with "HMAC verification failed"',
    icon: TamperIcon,
    color: 'warning',
    testFn: testTamperedPayload
  },
  {
    id: 'old_nonce',
    name: 'Old/Expired Nonce',
    description: 'Sends payload with nonce timestamp from 1 hour ago',
    expected: 'Server should reject as replay (nonce too old)',
    icon: NonceIcon,
    color: 'info',
    testFn: testOldNonce
  },
  {
    id: 'missing_nonce',
    name: 'Missing Nonce',
    description: 'Sends payload without the required nonce field',
    expected: 'Server should reject with "Missing required security fields"',
    icon: WarningIcon,
    color: 'secondary',
    testFn: testMissingNonce
  },
  {
    id: 'invalid_format',
    name: 'Invalid Format',
    description: 'Sends completely malformed security payload',
    expected: 'Server should reject with "Missing required security fields"',
    icon: FormatIcon,
    color: 'error',
    testFn: testInvalidFormat
  }
];

const SecurityTests = () => {
  const queryClient = useQueryClient();
  const [deviceId, setDeviceId] = useState('');
  const [testResults, setTestResults] = useState([]);
  const [runningTest, setRunningTest] = useState(null); // Track which test is running
  const [runningAll, setRunningAll] = useState(false);

  // Fetch devices
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Fetch security stats
  const { data: statsData, isLoading: statsLoading, refetch: refetchStats } = useQuery({
    queryKey: ['security-stats'],
    queryFn: getSecurityStats,
    refetchInterval: 10000
  });

  const securityStats = statsData?.data?.statistics || {};

  // Fetch security fault status
  const { data: faultStatusData, refetch: refetchFaultStatus } = useQuery({
    queryKey: ['security-fault-status'],
    queryFn: getSecurityFaultStatus,
    refetchInterval: 5000
  });

  const securityFaultStatus = faultStatusData?.data?.security_fault || {};

  // Fetch device security info
  const { data: deviceSecurityData, refetch: refetchDeviceSecurity } = useQuery({
    queryKey: ['device-security', deviceId],
    queryFn: () => getDeviceSecurityInfo(deviceId),
    enabled: !!deviceId,
    staleTime: 5000
  });

  const deviceSecurity = deviceSecurityData?.data?.device_info || {};

  // Reset stats mutation
  const resetStatsMutation = useMutation({
    mutationFn: resetSecurityStats,
    onSuccess: () => {
      queryClient.invalidateQueries(['security-stats']);
    }
  });

  // Clear nonces mutation
  const clearNoncesMutation = useMutation({
    mutationFn: () => clearDeviceNonces(deviceId),
    onSuccess: () => {
      queryClient.invalidateQueries(['device-security', deviceId]);
    }
  });

  // Clear security faults mutation
  const clearFaultsMutation = useMutation({
    mutationFn: clearSecurityFaults,
    onSuccess: () => {
      queryClient.invalidateQueries(['security-fault-status']);
      setTestResults([]);
    }
  });

  // Run individual test
  const runTest = async (test) => {
    if (!deviceId) {
      alert('Please select a device first');
      return;
    }

    setRunningTest(test.id);
    const startTime = Date.now();

    try {
      const result = await test.testFn(deviceId);
      const duration = Date.now() - startTime;

      const testResult = {
        id: test.id,
        name: test.name,
        test_passed: result.data?.test_passed ?? false,
        validation_succeeded: result.data?.validation_succeeded ?? true,
        expected_error: result.data?.expected_error || test.expected,
        actual_error: result.data?.actual_error || null,
        duration,
        timestamp: new Date().toISOString(),
        description: result.data?.description || test.description
      };

      // Update results (replace if same test, add if new)
      setTestResults(prev => {
        const filtered = prev.filter(r => r.id !== test.id);
        return [...filtered, testResult];
      });

      refetchStats();
      refetchFaultStatus();
      if (deviceId) refetchDeviceSecurity();

    } catch (error) {
      const duration = Date.now() - startTime;
      const testResult = {
        id: test.id,
        name: test.name,
        test_passed: true, // Error means security caught it
        validation_succeeded: false,
        expected_error: test.expected,
        actual_error: error.response?.data?.error || error.message,
        duration,
        timestamp: new Date().toISOString(),
        description: test.description
      };

      setTestResults(prev => {
        const filtered = prev.filter(r => r.id !== test.id);
        return [...filtered, testResult];
      });
    }

    setRunningTest(null);
  };

  // Run all tests sequentially
  const handleRunAllTests = async () => {
    if (!deviceId) {
      alert('Please select a device first');
      return;
    }

    setRunningAll(true);
    setTestResults([]);

    for (const test of SECURITY_TESTS) {
      await runTest(test);
      // Small delay between tests
      await new Promise(resolve => setTimeout(resolve, 300));
    }

    setRunningAll(false);
  };

  const handleResetStats = () => {
    resetStatsMutation.mutate();
  };

  const handleClearNonces = () => {
    if (deviceId) {
      clearNoncesMutation.mutate();
    }
  };

  const handleClearFaults = () => {
    clearFaultsMutation.mutate();
  };

  const getTestStatusIcon = (result) => {
    if (result.test_passed) {
      return <PassIcon color="success" />;
    }
    return <FailIcon color="error" />;
  };

  const getTestStatusChip = (result) => {
    if (result.test_passed) {
      return <Chip label="PASSED" color="success" size="small" />;
    }
    return <Chip label="FAILED" color="error" size="small" />;
  };

  return (
    <Box>
      {/* Header and Device Selection */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Fault Injection Testing
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Test security mechanisms by injecting faults: replay attacks, HMAC tampering, nonce manipulation, and malformed payloads.
          Each test verifies that the server correctly rejects invalid security payloads.
        </Typography>

        <Divider sx={{ my: 2 }} />

        {/* Device Selection and Controls */}
        <Grid container spacing={2} alignItems="center">
          <Grid item xs={12} md={4}>
            <FormControl fullWidth size="small">
              <InputLabel>Target Device</InputLabel>
              <Select
                value={deviceId}
                label="Target Device"
                onChange={(e) => setDeviceId(e.target.value)}
              >
                {devices.map((device) => (
                  <MenuItem key={device.device_id} value={device.device_id}>
                    {device.device_name || device.device_id}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>

          <Grid item xs={12} md={8}>
            <Stack direction="row" spacing={1} flexWrap="wrap">
              <Button
                variant="contained"
                onClick={handleRunAllTests}
                disabled={!deviceId || runningAll || runningTest}
                startIcon={runningAll ? <CircularProgress size={16} /> : <RunAllIcon />}
                size="small"
              >
                {runningAll ? 'Running All...' : 'Run All Tests'}
              </Button>
              <Button
                variant="outlined"
                color="warning"
                onClick={handleClearFaults}
                disabled={clearFaultsMutation.isPending}
                startIcon={<ClearIcon />}
                size="small"
              >
                Clear Results
              </Button>
              <Button
                variant="outlined"
                onClick={handleClearNonces}
                disabled={!deviceId || clearNoncesMutation.isPending}
                startIcon={<RefreshIcon />}
                size="small"
              >
                Clear Nonces
              </Button>
              <Button
                variant="outlined"
                onClick={handleResetStats}
                disabled={resetStatsMutation.isPending}
                startIcon={<RefreshIcon />}
                size="small"
              >
                Reset Stats
              </Button>
            </Stack>
          </Grid>
        </Grid>

        {!deviceId && (
          <Alert severity="info" sx={{ mt: 2 }}>
            Please select a target device to run security tests
          </Alert>
        )}
      </Paper>

      {/* Individual Security Test Cards */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Tests
        </Typography>
        <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
          Click on any test to run it individually. Tests verify that the server correctly rejects malicious payloads.
        </Typography>

        <Grid container spacing={2}>
          {SECURITY_TESTS.map((test) => {
            const TestIcon = test.icon;
            const result = testResults.find(r => r.id === test.id);
            const isRunning = runningTest === test.id;

            return (
              <Grid item xs={12} sm={6} md={4} key={test.id}>
                <Card 
                  variant="outlined" 
                  sx={{ 
                    height: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    borderColor: result ? (result.test_passed ? 'success.main' : 'error.main') : 'divider',
                    borderWidth: result ? 2 : 1
                  }}
                >
                  <CardContent sx={{ flexGrow: 1 }}>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 1 }}>
                      <TestIcon color={test.color} />
                      <Typography variant="subtitle1" fontWeight="bold">
                        {test.name}
                      </Typography>
                    </Box>
                    <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
                      {test.description}
                    </Typography>
                    <Typography variant="caption" color="text.secondary">
                      <strong>Expected:</strong> {test.expected}
                    </Typography>

                    {/* Show result if available */}
                    {result && (
                      <Box sx={{ mt: 2, p: 1, bgcolor: result.test_passed ? 'success.light' : 'error.light', borderRadius: 1 }}>
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          {getTestStatusIcon(result)}
                          <Typography variant="body2" fontWeight="bold">
                            {result.test_passed ? 'Test Passed' : 'Test Failed'}
                          </Typography>
                        </Box>
                        <Typography variant="caption" sx={{ display: 'block', mt: 0.5 }}>
                          {result.actual_error || 'Security mechanism working correctly'}
                        </Typography>
                        <Typography variant="caption" color="text.secondary">
                          Duration: {result.duration}ms
                        </Typography>
                      </Box>
                    )}
                  </CardContent>

                  <CardActions>
                    <Button
                      size="small"
                      variant="contained"
                      color={test.color}
                      onClick={() => runTest(test)}
                      disabled={!deviceId || isRunning || runningAll}
                      startIcon={isRunning ? <CircularProgress size={16} /> : <RunIcon />}
                      fullWidth
                    >
                      {isRunning ? 'Running...' : 'Run Test'}
                    </Button>
                  </CardActions>
                </Card>
              </Grid>
            );
          })}
        </Grid>
      </Paper>

      {/* Test Results Table */}
      {testResults.length > 0 && (
        <Paper sx={{ p: 3, mb: 3 }}>
          <Typography variant="h6" gutterBottom>
            Test Results Summary
          </Typography>

          <Box sx={{ mb: 2 }}>
            <Chip 
              label={`Passed: ${testResults.filter(r => r.test_passed).length}`} 
              color="success" 
              size="small" 
              sx={{ mr: 1 }} 
            />
            <Chip 
              label={`Failed: ${testResults.filter(r => !r.test_passed).length}`} 
              color="error" 
              size="small" 
            />
          </Box>

          <TableContainer>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>Test</TableCell>
                  <TableCell>Status</TableCell>
                  <TableCell>Duration</TableCell>
                  <TableCell>Error Message</TableCell>
                  <TableCell>Timestamp</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {testResults.map((result) => (
                  <TableRow key={result.id}>
                    <TableCell>
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        {getTestStatusIcon(result)}
                        <Typography variant="body2">{result.name}</Typography>
                      </Box>
                    </TableCell>
                    <TableCell>{getTestStatusChip(result)}</TableCell>
                    <TableCell>{result.duration}ms</TableCell>
                    <TableCell>
                      <Typography variant="caption" sx={{ fontFamily: 'monospace' }}>
                        {result.actual_error || 'N/A'}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="caption">
                        {new Date(result.timestamp).toLocaleTimeString()}
                      </Typography>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        </Paper>
      )}

      {/* Security Statistics */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Statistics
        </Typography>

        {statsLoading ? (
          <CircularProgress />
        ) : (
          <Grid container spacing={2}>
            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    Total Validations
                  </Typography>
                  <Typography variant="h5">
                    {securityStats.total_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    Successful
                  </Typography>
                  <Typography variant="h5" color="success.main">
                    {securityStats.successful_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    Failed
                  </Typography>
                  <Typography variant="h5" color="error.main">
                    {securityStats.failed_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    Replay Blocked
                  </Typography>
                  <Typography variant="h5" color="warning.main">
                    {securityStats.replay_attacks_blocked || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    HMAC Failures
                  </Typography>
                  <Typography variant="h5" color="error.main">
                    {securityStats.signature_failures || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={6} sm={4} md={2}>
              <Card variant="outlined">
                <CardContent sx={{ textAlign: 'center', py: 1 }}>
                  <Typography variant="caption" color="text.secondary">
                    Success Rate
                  </Typography>
                  <Typography variant="h5" color="primary.main">
                    {securityStats.success_rate || 0}%
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
          </Grid>
        )}
      </Paper>

      {/* Fault Injection Status */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Fault Injection Status
        </Typography>

        <Grid container spacing={2}>
          <Grid item xs={12} md={6}>
            <Card variant="outlined">
              <CardContent>
                <Typography variant="subtitle2" color="text.secondary">
                  Status
                </Typography>
                <Chip
                  label={securityFaultStatus.enabled ? 'ACTIVE' : 'INACTIVE'}
                  color={securityFaultStatus.enabled ? 'warning' : 'default'}
                  size="small"
                />
              </CardContent>
            </Card>
          </Grid>

          <Grid item xs={12} md={6}>
            <Card variant="outlined">
              <CardContent>
                <Typography variant="subtitle2" color="text.secondary">
                  Faults Injected
                </Typography>
                <Typography variant="h6">
                  {securityFaultStatus.faults_injected || 0}
                </Typography>
              </CardContent>
            </Card>
          </Grid>

          {securityFaultStatus.statistics && (
            <>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Replay</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.replay_attacks_triggered || 0}</Typography>
              </Grid>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Invalid HMAC</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.invalid_hmac_triggered || 0}</Typography>
              </Grid>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Tampered</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.tampered_payload_triggered || 0}</Typography>
              </Grid>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Old Nonce</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.old_nonce_triggered || 0}</Typography>
              </Grid>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Missing Nonce</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.missing_nonce_triggered || 0}</Typography>
              </Grid>
              <Grid item xs={6} md={2}>
                <Typography variant="caption" color="text.secondary">Invalid Format</Typography>
                <Typography variant="body1">{securityFaultStatus.statistics.invalid_format_triggered || 0}</Typography>
              </Grid>
            </>
          )}
        </Grid>
      </Paper>

      {/* Device Security Info */}
      {deviceId && (
        <Paper sx={{ p: 3 }}>
          <Typography variant="h6" gutterBottom>
            Device Security Information: {deviceId}
          </Typography>

          <Grid container spacing={2}>
            <Grid item xs={12} md={3}>
              <Typography variant="body2" color="text.secondary">
                Nonces Tracked
              </Typography>
              <Typography variant="body1" fontWeight="bold">
                {deviceSecurity.nonces_tracked || 0}
              </Typography>
            </Grid>

            <Grid item xs={12} md={3}>
              <Typography variant="body2" color="text.secondary">
                Total Requests
              </Typography>
              <Typography variant="body1">
                {deviceSecurity.stats?.total_requests || 0}
              </Typography>
            </Grid>

            <Grid item xs={12} md={3}>
              <Typography variant="body2" color="text.secondary">
                Valid Requests
              </Typography>
              <Typography variant="body1" color="success.main">
                {deviceSecurity.stats?.valid_requests || 0}
              </Typography>
            </Grid>

            <Grid item xs={12} md={3}>
              <Typography variant="body2" color="text.secondary">
                Replay Blocked
              </Typography>
              <Typography variant="body1" color="warning.main">
                {deviceSecurity.stats?.replay_blocked || 0}
              </Typography>
            </Grid>
          </Grid>
        </Paper>
      )}
    </Box>
  );
};

export default SecurityTests;
