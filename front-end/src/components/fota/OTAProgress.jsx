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
  
  // Get OTA check interval from device-specific status (default to 60 minutes if not available)
  // This uses the device's actual firmware_check_interval config value
  const otaCheckIntervalMinutes = status.ota_check_interval_minutes || 60;
  
  // Format interval for display
  const formatInterval = (minutes) => {
    if (minutes >= 60) {
      const hours = minutes / 60;
      return `${hours} ${hours === 1 ? 'hour' : 'hours'}`;
    }
    return `${minutes} ${minutes === 1 ? 'minute' : 'minutes'}`;
  };

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

  const getStatusIcon = (status) => {
    switch (status) {
      case 'in_progress':
        return <DownloadIcon color="primary" />;
      case 'completed':
        return <SuccessIcon color="success" />;
      case 'failed':
        return <ErrorIcon color="error" />;
      default:
        return <WaitingIcon color="disabled" />;
    }
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'in_progress':
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
    // Use progress_percent from backend if available
    if (status.progress_percent !== undefined) {
      return status.progress_percent;
    }
    
    // Otherwise use current_chunk and total_chunks to calculate
    if (status.current_chunk !== undefined && status.total_chunks > 0) {
      return Math.round((status.current_chunk / status.total_chunks) * 100);
    }
    
    // Default based on status
    if (status.status === 'completed') return 100;
    if (status.status === 'in_progress') return 0;
    return 0;
  };

  const isActive = status.status === 'in_progress';

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
            {/* Current Firmware Version */}
            <Box sx={{ mb: 3, p: 2, bgcolor: 'grey.50', borderRadius: 1 }}>
              <Typography variant="subtitle2" color="text.secondary" gutterBottom>
                Current Firmware Version
              </Typography>
              <Typography variant="h6" fontWeight="bold">
                {status.current_firmware_version || 'unknown'}
              </Typography>
            </Box>

            {/* Current Status */}
            <Stack direction="row" spacing={2} alignItems="center" sx={{ mb: 3 }}>
              {getStatusIcon(status.status)}
              <Box sx={{ flexGrow: 1 }}>
                <Typography variant="subtitle1" fontWeight="bold">
                  Status: {status.status || 'idle'}
                </Typography>
                {status.firmware_version && (
                  <Typography variant="body2" color="text.secondary">
                    Target version: {status.firmware_version}
                  </Typography>
                )}
              </Box>
              <Chip 
                label={status.status || 'idle'}
                color={getStatusColor(status.status)}
                icon={getStatusIcon(status.status)}
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

            {/* Additional Info / Phase Message */}
            {status.message && (
              <Alert severity="info" sx={{ mb: 2 }}>
                {status.message}
              </Alert>
            )}
            
            {/* Phase-specific messages */}
            {status.phase === 'download_complete' && (
              <Alert severity="success" sx={{ mb: 2 }}>
                ✓ Download completed successfully
              </Alert>
            )}
            
            {status.phase === 'verifying' && (
              <Alert severity="info" sx={{ mb: 2 }}>
                🔒 Verifying firmware security...
              </Alert>
            )}
            
            {status.phase === 'verification_success' && (
              <Alert severity="success" sx={{ mb: 2 }}>
                ✓ Security verification passed - Ready to install
              </Alert>
            )}
            
            {status.phase === 'verification_failed' && (
              <Alert severity="error" sx={{ mb: 2 }}>
                ✗ Security verification failed - Rolling back to previous version
              </Alert>
            )}
            
            {status.phase === 'installing' && (
              <Alert severity="warning" sx={{ mb: 2 }}>
                ⚙️ Installing new firmware - Device will reboot soon
              </Alert>
            )}
            
            {status.phase === 'rollback' && (
              <Alert severity="warning" sx={{ mb: 2 }}>
                ↩️ Rolling back to previous firmware version
              </Alert>
            )}

            {/* Chunk Progress */}
            {status.current_chunk !== undefined && status.total_chunks && (
              <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
                Downloading chunk {status.current_chunk + 1} of {status.total_chunks}
              </Typography>
            )}

            {/* Success Message */}
            {status.status === 'completed' && (
              <Alert severity="success" icon={<SuccessIcon />} sx={{ mb: 2 }}>
                OTA update completed successfully! Device is now running version {status.firmware_version}
              </Alert>
            )}

            {/* Error Message */}
            {status.status === 'failed' && (
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
            {(!status.status || status.error === 'No active OTA session') && (
              <Alert severity="info">
                No active OTA update. ESP32 devices automatically check for updates every {formatInterval(otaCheckIntervalMinutes)}.
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
