/**
 * OTAHistory Component
 * 
 * Displays OTA update history from database
 * Features:
 * - View past OTA updates for a device
 * - Filter by device
 * - Status indicators (completed, failed, etc.)
 * - Detailed information about each update
 */

import React, { useState, useEffect } from 'react';
import {
  Box,
  Paper,
  Typography,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Chip,
  Alert,
  CircularProgress,
  Tooltip
} from '@mui/material';
import {
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  HourglassEmpty as PendingIcon,
  Cancel as CancelIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getOTAHistory } from '../../api/ota';
import { getDevices } from '../../api/devices';

const OTAHistory = () => {
  const [selectedDevice, setSelectedDevice] = useState('');

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

  // Fetch OTA history
  const {
    data: historyData,
    isLoading,
    isError,
    error
  } = useQuery({
    queryKey: ['otaHistory', selectedDevice],
    queryFn: () => getOTAHistory(selectedDevice, 50),
    enabled: !!selectedDevice,
    staleTime: 10000
  });

  const history = historyData?.data?.history || [];

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
  };

  const getStatusIcon = (status) => {
    switch (status) {
      case 'completed':
        return <SuccessIcon fontSize="small" />;
      case 'failed':
      case 'error':
        return <ErrorIcon fontSize="small" />;
      case 'cancelled':
        return <CancelIcon fontSize="small" />;
      default:
        return <PendingIcon fontSize="small" />;
    }
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'completed':
        return 'success';
      case 'failed':
      case 'error':
        return 'error';
      case 'cancelled':
        return 'default';
      case 'initiated':
      case 'downloading':
      case 'installing':
        return 'warning';
      default:
        return 'default';
    }
  };

  const formatDate = (dateString) => {
    if (!dateString) return 'N/A';
    const date = new Date(dateString);
    return date.toLocaleString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  const formatDuration = (startDate, endDate) => {
    if (!startDate || !endDate) return 'N/A';
    const start = new Date(startDate);
    const end = new Date(endDate);
    const durationMs = end - start;
    const durationSec = Math.floor(durationMs / 1000);
    
    if (durationSec < 60) {
      return `${durationSec}s`;
    } else if (durationSec < 3600) {
      const minutes = Math.floor(durationSec / 60);
      const seconds = durationSec % 60;
      return `${minutes}m ${seconds}s`;
    } else {
      const hours = Math.floor(durationSec / 3600);
      const minutes = Math.floor((durationSec % 3600) / 60);
      return `${hours}h ${minutes}m`;
    }
  };

  return (
    <Box sx={{ p: 3 }}>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          OTA Update History
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom sx={{ mb: 3 }}>
          View past firmware update attempts and their outcomes
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
            Error loading OTA history: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Empty State */}
        {!isLoading && !isError && history.length === 0 && (
          <Alert severity="info">
            No OTA update history found for this device.
          </Alert>
        )}

        {/* History Table */}
        {!isLoading && !isError && history.length > 0 && (
          <TableContainer>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell>Status</TableCell>
                  <TableCell>From Version</TableCell>
                  <TableCell>To Version</TableCell>
                  <TableCell>Initiated At</TableCell>
                  <TableCell>Duration</TableCell>
                  <TableCell>Progress</TableCell>
                  <TableCell>Error</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {history.map((update) => (
                  <TableRow key={update.id} hover>
                    <TableCell>
                      <Chip
                        icon={getStatusIcon(update.status)}
                        label={update.status}
                        color={getStatusColor(update.status)}
                        size="small"
                      />
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" fontFamily="monospace">
                        {update.from_version}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" fontFamily="monospace">
                        {update.to_version}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2">
                        {formatDate(update.initiated_at)}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2">
                        {formatDuration(
                          update.initiated_at,
                          update.install_completed_at || update.download_completed_at
                        )}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      {update.chunks_downloaded !== null && update.chunks_total !== null ? (
                        <Tooltip title={`${update.chunks_downloaded} / ${update.chunks_total} chunks`}>
                          <Typography variant="body2">
                            {update.chunks_total > 0
                              ? `${Math.round((update.chunks_downloaded / update.chunks_total) * 100)}%`
                              : 'N/A'}
                          </Typography>
                        </Tooltip>
                      ) : (
                        <Typography variant="body2" color="text.secondary">
                          N/A
                        </Typography>
                      )}
                    </TableCell>
                    <TableCell>
                      {update.error_msg ? (
                        <Tooltip title={update.error_msg}>
                          <Typography
                            variant="body2"
                            color="error"
                            sx={{
                              maxWidth: 200,
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              whiteSpace: 'nowrap'
                            }}
                          >
                            {update.error_msg}
                          </Typography>
                        </Tooltip>
                      ) : (
                        <Typography variant="body2" color="text.secondary">
                          -
                        </Typography>
                      )}
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        )}

        {/* Record Count */}
        {!isLoading && !isError && history.length > 0 && (
          <Box sx={{ mt: 2, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <Typography variant="body2" color="text.secondary">
              Showing {history.length} update{history.length !== 1 ? 's' : ''}
            </Typography>
            <Typography variant="body2" color="text.secondary">
              {history.filter(u => u.status === 'completed').length} completed, {' '}
              {history.filter(u => u.status === 'failed' || u.status === 'error').length} failed
            </Typography>
          </Box>
        )}
      </Paper>
    </Box>
  );
};

export default OTAHistory;
