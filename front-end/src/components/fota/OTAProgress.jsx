/**
 * OTAProgress Component
 * 
 * Displays OTA update progress and status
 * Features:
 * - Real-time OTA status monitoring
 * - Progress bar for chunk download
 * - Status indicators (idle, downloading, verifying, installing)
 * - Success/failure notifications
 * - Cancel OTA option
 */

import React, { useState, useEffect } from 'react';
import {
  Box,
  Paper,
  Typography,
  LinearProgress,
  Alert,
  Button,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Chip,
  Stack,
  Divider,
  CircularProgress
} from '@mui/material';
import {
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  HourglassEmpty as WaitingIcon,
  CloudDownload as DownloadIcon,
  VerifiedUser as VerifyIcon,
  SystemUpdate as InstallIcon,
  Cancel as CancelIcon
} from '@mui/icons-material';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getOTAStatus, cancelOTA, getOTAStats } from '../../api/ota';
import { getDevices } from '../../api/devices';

const OTAProgress = () => {
  const [selectedDevice, setSelectedDevice] = useState('');
  const queryClient = useQueryClient();

  // Fetch devices
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Auto-select first device
  useEffect(() => {
    if (devices.length > 0 && !selectedDevice) {
      setSelectedDevice(devices[0].device_id);
    }
  }, [devices, selectedDevice]);

  // Fetch OTA status with auto-refresh
  const {
    data: statusData,
    isLoading,
    isError
  } = useQuery({
    queryKey: ['otaStatus', selectedDevice],
    queryFn: () => getOTAStatus(selectedDevice),
    enabled: !!selectedDevice,
    refetchInterval: 2000, // Poll every 2 seconds
    staleTime: 1000
  });

  // Fetch OTA statistics
  const { data: statsData } = useQuery({
    queryKey: ['otaStats'],
    queryFn: getOTAStats,
    refetchInterval: 5000,
    staleTime: 3000
  });

  const status = statusData?.data?.status || {};
  const stats = statsData?.data || {};

  // Cancel OTA mutation
  const cancelMutation = useMutation({
    mutationFn: (deviceId) => cancelOTA(deviceId),
    onSuccess: () => {
      queryClient.invalidateQueries(['otaStatus']);
    }
  });

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
  };

  const handleCancelOTA = () => {
    if (window.confirm('Are you sure you want to cancel the OTA update?')) {
      cancelMutation.mutate(selectedDevice);
    }
  };

  const getStatusIcon = (state) => {
    switch (state) {
      case 'downloading':
        return <DownloadIcon color="primary" />;
      case 'verifying':
        return <VerifyIcon color="info" />;
      case 'installing':
        return <InstallIcon color="warning" />;
      case 'completed':
        return <SuccessIcon color="success" />;
      case 'failed':
        return <ErrorIcon color="error" />;
      default:
        return <WaitingIcon color="disabled" />;
    }
  };

  const getStatusColor = (state) => {
    switch (state) {
      case 'downloading':
      case 'verifying':
      case 'installing':
        return 'primary';
      case 'completed':
        return 'success';
      case 'failed':
        return 'error';
      default:
        return 'default';
    }
  };

  const getProgressValue = () => {
    if (!status.state || status.state === 'idle') return 0;
    if (status.state === 'completed') return 100;
    if (status.progress !== undefined) return status.progress;
    
    // Estimate progress based on state
    switch (status.state) {
      case 'downloading': return 33;
      case 'verifying': return 66;
      case 'installing': return 90;
      default: return 0;
    }
  };

  const isActive = status.state && 
    status.state !== 'idle' && 
    status.state !== 'completed' && 
    status.state !== 'failed';

  return (
    <Box sx={{ p: 3 }}>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          OTA Update Progress
        </Typography>

        {/* Device Selector */}
        <FormControl fullWidth sx={{ mb: 3 }}>
          <InputLabel>Device</InputLabel>
          <Select
            value={selectedDevice}
            label="Device"
            onChange={handleDeviceChange}
          >
            {devices.map((device) => (
              <MenuItem key={device.device_id} value={device.device_id}>
                {device.device_name || device.device_id} (v{device.firmware_version || 'unknown'})
              </MenuItem>
            ))}
          </Select>
        </FormControl>

        {/* Loading State */}
        {isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading OTA status
          </Alert>
        )}

        {/* Status Display */}
        {!isLoading && !isError && selectedDevice && (
          <Box>
            {/* Current Status */}
            <Stack direction="row" spacing={2} alignItems="center" sx={{ mb: 3 }}>
              {getStatusIcon(status.state)}
              <Box sx={{ flexGrow: 1 }}>
                <Typography variant="subtitle1" fontWeight="bold">
                  Status: {status.state || 'idle'}
                </Typography>
                {status.target_version && (
                  <Typography variant="body2" color="text.secondary">
                    Target version: {status.target_version}
                  </Typography>
                )}
              </Box>
              <Chip 
                label={status.state || 'idle'}
                color={getStatusColor(status.state)}
                icon={getStatusIcon(status.state)}
              />
            </Stack>

            {/* Progress Bar */}
            <Box sx={{ mb: 3 }}>
              <Box sx={{ display: 'flex', justifyContent: 'space-between', mb: 1 }}>
                <Typography variant="body2" color="text.secondary">
                  Progress
                </Typography>
                <Typography variant="body2" color="text.secondary">
                  {getProgressValue()}%
                </Typography>
              </Box>
              <LinearProgress 
                variant="determinate" 
                value={getProgressValue()} 
                sx={{ height: 10, borderRadius: 5 }}
              />
            </Box>

            {/* Additional Info */}
            {status.message && (
              <Alert severity="info" sx={{ mb: 2 }}>
                {status.message}
              </Alert>
            )}

            {/* Chunk Progress */}
            {status.current_chunk !== undefined && status.total_chunks && (
              <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
                Downloading chunk {status.current_chunk + 1} of {status.total_chunks}
              </Typography>
            )}

            {/* Success Message */}
            {status.state === 'completed' && (
              <Alert severity="success" icon={<SuccessIcon />} sx={{ mb: 2 }}>
                OTA update completed successfully! Device is now running version {status.target_version}
              </Alert>
            )}

            {/* Error Message */}
            {status.state === 'failed' && (
              <Alert severity="error" icon={<ErrorIcon />} sx={{ mb: 2 }}>
                OTA update failed: {status.error || 'Unknown error'}
              </Alert>
            )}

            {/* Cancel Button */}
            {isActive && (
              <Button
                variant="outlined"
                color="error"
                onClick={handleCancelOTA}
                disabled={cancelMutation.isPending}
                startIcon={<CancelIcon />}
                fullWidth
              >
                Cancel Update
              </Button>
            )}

            {/* Idle State Message */}
            {(!status.state || status.state === 'idle') && (
              <Alert severity="info">
                No active OTA update. Go to "Manage Firmware" to initiate an update.
              </Alert>
            )}
          </Box>
        )}

        <Divider sx={{ my: 3 }} />

        {/* Statistics */}
        <Box>
          <Typography variant="h6" gutterBottom>
            OTA Statistics
          </Typography>
          
          <Stack spacing={2}>
            <Box sx={{ display: 'flex', justifyContent: 'space-between' }}>
              <Typography variant="body2" color="text.secondary">
                Total Updates:
              </Typography>
              <Typography variant="body2" fontWeight="bold">
                {stats.total_updates || 0}
              </Typography>
            </Box>
            
            <Box sx={{ display: 'flex', justifyContent: 'space-between' }}>
              <Typography variant="body2" color="text.secondary">
                Successful:
              </Typography>
              <Typography variant="body2" fontWeight="bold" color="success.main">
                {stats.successful_updates || 0}
              </Typography>
            </Box>
            
            <Box sx={{ display: 'flex', justifyContent: 'space-between' }}>
              <Typography variant="body2" color="text.secondary">
                Failed:
              </Typography>
              <Typography variant="body2" fontWeight="bold" color="error.main">
                {stats.failed_updates || 0}
              </Typography>
            </Box>
            
            <Box sx={{ display: 'flex', justifyContent: 'space-between' }}>
              <Typography variant="body2" color="text.secondary">
                Active Sessions:
              </Typography>
              <Typography variant="body2" fontWeight="bold">
                {stats.active_sessions || 0}
              </Typography>
            </Box>
            
            {stats.success_rate !== undefined && (
              <Box sx={{ display: 'flex', justifyContent: 'space-between' }}>
                <Typography variant="body2" color="text.secondary">
                  Success Rate:
                </Typography>
                <Typography variant="body2" fontWeight="bold">
                  {Math.round(stats.success_rate * 100)}%
                </Typography>
              </Box>
            )}
          </Stack>
        </Box>
      </Paper>
    </Box>
  );
};

export default OTAProgress;
