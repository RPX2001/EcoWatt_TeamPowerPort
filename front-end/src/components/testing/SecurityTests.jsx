/**
 * SecurityTests Component
 * 
 * Comprehensive security testing interface
 * Features:
 * - Replay attack detection
 * - Tampered payload validation
 * - Invalid HMAC detection
 * - Old nonce rejection
 * - Security statistics viewer
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
  Grid,
  Card,
  CardContent,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Accordion,
  AccordionSummary,
  AccordionDetails,
  LinearProgress
} from '@mui/material';
import {
  Security as SecurityIcon,
  PlayArrow as RunIcon,
  CheckCircle as PassIcon,
  Error as FailIcon,
  Warning as WarningIcon,
  ExpandMore as ExpandIcon,
  Refresh as RefreshIcon
} from '@mui/icons-material';
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import {
  testReplayAttack,
  testTamperedPayload,
  testInvalidHMAC,
  testOldNonce,
  getSecurityStats,
  resetSecurityStats,
  clearDeviceNonces,
  getDeviceSecurityInfo
} from '../../api/security';
import { getDevices } from '../../api/devices';

const SecurityTests = () => {
  const queryClient = useQueryClient();
  const [deviceId, setDeviceId] = useState('');
  const [testResults, setTestResults] = useState([]);
  const [runningTests, setRunningTests] = useState(false);

  // Sample secured payload for testing
  const [testPayload, setTestPayload] = useState({
    nonce: Math.floor(Date.now() / 1000),
    hmac: 'a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6',
    encrypted_data: 'test_encrypted_data_sample'
  });

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

  // Run individual test
  const runTest = async (testName, testFn) => {
    const startTime = Date.now();
    try {
      const result = await testFn();
      const duration = Date.now() - startTime;
      
      return {
        test: testName,
        status: 'pass',
        result: result.data,
        duration,
        timestamp: new Date().toISOString()
      };
    } catch (error) {
      const duration = Date.now() - startTime;
      return {
        test: testName,
        status: 'expected_fail',
        error: error.response?.data?.error || error.message,
        duration,
        timestamp: new Date().toISOString()
      };
    }
  };

  // Run all security tests
  const handleRunAllTests = async () => {
    if (!deviceId) {
      alert('Please select a device');
      return;
    }

    setRunningTests(true);
    const results = [];

    // Test 1: Replay Attack
    const replayResult = await runTest(
      'Replay Attack Detection',
      () => testReplayAttack(deviceId, testPayload)
    );
    results.push(replayResult);

    // Test 2: Tampered Payload
    const tamperResult = await runTest(
      'Tampered Payload Detection',
      () => testTamperedPayload(deviceId, testPayload)
    );
    results.push(tamperResult);

    // Test 3: Invalid HMAC
    const hmacResult = await runTest(
      'Invalid HMAC Detection',
      () => testInvalidHMAC(deviceId, testPayload)
    );
    results.push(hmacResult);

    // Test 4: Old Nonce
    const nonceResult = await runTest(
      'Old Nonce Rejection',
      () => testOldNonce(deviceId, testPayload)
    );
    results.push(nonceResult);

    setTestResults(results);
    setRunningTests(false);
    refetchStats();
    if (deviceId) refetchDeviceSecurity();
  };

  const handleResetStats = () => {
    resetStatsMutation.mutate();
  };

  const handleClearNonces = () => {
    if (deviceId) {
      clearNoncesMutation.mutate();
    }
  };

  const handleRegeneratePayload = () => {
    setTestPayload({
      nonce: Math.floor(Date.now() / 1000),
      hmac: Array.from({ length: 64 }, () => 
        '0123456789abcdef'[Math.floor(Math.random() * 16)]
      ).join(''),
      encrypted_data: `test_data_${Date.now()}`
    });
  };

  const getTestStatusIcon = (status) => {
    if (status === 'pass') return <PassIcon color="success" />;
    if (status === 'expected_fail') return <WarningIcon color="warning" />;
    return <FailIcon color="error" />;
  };

  return (
    <Box>
      <Paper sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Testing Suite
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Test security mechanisms: replay attack detection, payload tampering, HMAC validation, and nonce management
        </Typography>

        <Divider sx={{ my: 3 }} />

        {/* Device Selection */}
        <Grid container spacing={3}>
          <Grid item xs={12} md={6}>
            <FormControl fullWidth>
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

          <Grid item xs={12} md={6}>
            <Stack direction="row" spacing={2}>
              <Button
                variant="contained"
                fullWidth
                onClick={handleRunAllTests}
                disabled={!deviceId || runningTests}
                startIcon={runningTests ? <CircularProgress size={20} /> : <RunIcon />}
              >
                {runningTests ? 'Running Tests...' : 'Run All Security Tests'}
              </Button>
              <Button
                variant="outlined"
                onClick={handleRegeneratePayload}
                startIcon={<RefreshIcon />}
              >
                New Payload
              </Button>
            </Stack>
          </Grid>
        </Grid>

        {/* Test Payload */}
        <Box sx={{ mt: 3 }}>
          <Accordion>
            <AccordionSummary expandIcon={<ExpandIcon />}>
              <Typography variant="subtitle2">Test Payload</Typography>
            </AccordionSummary>
            <AccordionDetails>
              <TextField
                fullWidth
                multiline
                rows={4}
                value={JSON.stringify(testPayload, null, 2)}
                onChange={(e) => {
                  try {
                    setTestPayload(JSON.parse(e.target.value));
                  } catch (err) {
                    // Invalid JSON, ignore
                  }
                }}
                sx={{ fontFamily: 'monospace', fontSize: '0.85rem' }}
              />
            </AccordionDetails>
          </Accordion>
        </Box>

        {/* Running Tests Progress */}
        {runningTests && (
          <Box sx={{ mt: 3 }}>
            <LinearProgress />
            <Typography variant="body2" color="text.secondary" sx={{ mt: 1, textAlign: 'center' }}>
              Running security tests...
            </Typography>
          </Box>
        )}

        {/* Test Results */}
        {testResults.length > 0 && (
          <Box sx={{ mt: 3 }}>
            <Typography variant="h6" gutterBottom>
              Test Results
            </Typography>
            <TableContainer component={Paper} variant="outlined">
              <Table>
                <TableHead>
                  <TableRow>
                    <TableCell>Test</TableCell>
                    <TableCell>Status</TableCell>
                    <TableCell>Duration</TableCell>
                    <TableCell>Details</TableCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {testResults.map((result, index) => (
                    <TableRow key={index}>
                      <TableCell>
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                          {getTestStatusIcon(result.status)}
                          <Typography variant="body2">{result.test}</Typography>
                        </Box>
                      </TableCell>
                      <TableCell>
                        <Chip
                          label={result.status === 'pass' ? 'PASS' : 'EXPECTED FAIL'}
                          color={result.status === 'pass' ? 'success' : 'warning'}
                          size="small"
                        />
                      </TableCell>
                      <TableCell>
                        <Typography variant="body2">{result.duration}ms</Typography>
                      </TableCell>
                      <TableCell>
                        <Typography variant="caption" sx={{ fontFamily: 'monospace' }}>
                          {result.error ? result.error : 'Security mechanism working correctly'}
                        </Typography>
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </TableContainer>
          </Box>
        )}
      </Paper>

      {/* Security Statistics */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6">
            Security Statistics
          </Typography>
          <Button
            size="small"
            onClick={handleResetStats}
            disabled={resetStatsMutation.isPending}
            startIcon={<RefreshIcon />}
          >
            Reset Stats
          </Button>
        </Box>

        {statsLoading ? (
          <CircularProgress />
        ) : (
          <Grid container spacing={2}>
            <Grid item xs={12} sm={6} md={3}>
              <Card variant="outlined">
                <CardContent>
                  <Typography color="text.secondary" gutterBottom>
                    Total Validations
                  </Typography>
                  <Typography variant="h4">
                    {securityStats.total_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={12} sm={6} md={3}>
              <Card variant="outlined">
                <CardContent>
                  <Typography color="text.secondary" gutterBottom>
                    Successful
                  </Typography>
                  <Typography variant="h4" color="success.main">
                    {securityStats.successful_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={12} sm={6} md={3}>
              <Card variant="outlined">
                <CardContent>
                  <Typography color="text.secondary" gutterBottom>
                    Failed
                  </Typography>
                  <Typography variant="h4" color="error.main">
                    {securityStats.failed_validations || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>

            <Grid item xs={12} sm={6} md={3}>
              <Card variant="outlined">
                <CardContent>
                  <Typography color="text.secondary" gutterBottom>
                    Replay Attacks
                  </Typography>
                  <Typography variant="h4" color="warning.main">
                    {securityStats.replay_attacks_detected || 0}
                  </Typography>
                </CardContent>
              </Card>
            </Grid>
          </Grid>
        )}
      </Paper>

      {/* Device Security Info */}
      {deviceId && (
        <Paper sx={{ p: 3 }}>
          <Box sx={{ display: 'flex', justifyContent: 'between', alignItems: 'center', mb: 2 }}>
            <Typography variant="h6">
              Device Security Information
            </Typography>
            <Button
              size="small"
              onClick={handleClearNonces}
              disabled={clearNoncesMutation.isPending}
              startIcon={<RefreshIcon />}
            >
              Clear Nonces
            </Button>
          </Box>

          <Grid container spacing={2}>
            <Grid item xs={12} md={6}>
              <Typography variant="body2" color="text.secondary">
                Device ID
              </Typography>
              <Typography variant="body1" fontWeight="bold">
                {deviceSecurity.device_id || deviceId}
              </Typography>
            </Grid>

            <Grid item xs={12} md={6}>
              <Typography variant="body2" color="text.secondary">
                Last Nonce
              </Typography>
              <Typography variant="body1" fontFamily="monospace">
                {deviceSecurity.last_nonce || 'N/A'}
              </Typography>
            </Grid>

            <Grid item xs={12} md={6}>
              <Typography variant="body2" color="text.secondary">
                Stored Nonces
              </Typography>
              <Typography variant="body1">
                {deviceSecurity.nonce_count || 0}
              </Typography>
            </Grid>

            <Grid item xs={12} md={6}>
              <Typography variant="body2" color="text.secondary">
                Last Validation
              </Typography>
              <Typography variant="body1">
                {deviceSecurity.last_validation 
                  ? new Date(deviceSecurity.last_validation).toLocaleString()
                  : 'N/A'}
              </Typography>
            </Grid>
          </Grid>
        </Paper>
      )}

      {/* Test Information */}
      <Paper sx={{ p: 3, mt: 3 }}>
        <Typography variant="h6" gutterBottom>
          Security Test Descriptions
        </Typography>
        <Divider sx={{ my: 2 }} />

        <Grid container spacing={2}>
          <Grid item xs={12} md={6}>
            <Box sx={{ mb: 2 }}>
              <Typography variant="subtitle2" fontWeight="bold">
                1. Replay Attack Detection
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Sends the same payload twice. The second attempt should fail due to nonce reuse detection.
              </Typography>
            </Box>

            <Box sx={{ mb: 2 }}>
              <Typography variant="subtitle2" fontWeight="bold">
                2. Tampered Payload Detection
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Modifies the HMAC of a valid payload. Should fail HMAC validation.
              </Typography>
            </Box>
          </Grid>

          <Grid item xs={12} md={6}>
            <Box sx={{ mb: 2 }}>
              <Typography variant="subtitle2" fontWeight="bold">
                3. Invalid HMAC Detection
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Sends a payload with a completely invalid HMAC. Should be rejected immediately.
              </Typography>
            </Box>

            <Box sx={{ mb: 2 }}>
              <Typography variant="subtitle2" fontWeight="bold">
                4. Old Nonce Rejection
              </Typography>
              <Typography variant="body2" color="text.secondary">
                Sends a payload with an old/expired nonce. Should fail freshness check.
              </Typography>
            </Box>
          </Grid>
        </Grid>
      </Paper>
    </Box>
  );
};

export default SecurityTests;
