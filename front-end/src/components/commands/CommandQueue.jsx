/**
 * CommandQueue Component
 * 
 * Displays pending commands in a real-time table
 * Features:
 * - Auto-refresh every 10 seconds
 * - Filter by device
 * - Cancel pending commands
 * - Status indicators
 */

import React, { useState, useEffect } from 'react';
import {
  Box,
  Paper,
  Typography,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  IconButton,
  Chip,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Alert,
  CircularProgress,
  Tooltip,
  Stack
} from '@mui/material';
import {
  Cancel as CancelIcon,
  Refresh as RefreshIcon,
  Timer as TimerIcon
} from '@mui/icons-material';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { pollCommands } from '../../api/commands';
import { getDevices } from '../../api/devices';

const CommandQueue = () => {
  const [selectedDevice, setSelectedDevice] = useState('all');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const queryClient = useQueryClient();

  // Fetch devices for filter
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Fetch pending commands with auto-refresh
  const {
    data: commandsData,
    isLoading,
    isError,
    error,
    refetch
  } = useQuery({
    queryKey: ['pendingCommands', selectedDevice],
    queryFn: async () => {
      if (selectedDevice === 'all') {
        // Fetch for all devices
        const allCommands = await Promise.all(
          devices.map(device => 
            pollCommands(device.device_id)
              .then(res => ({
                device_id: device.device_id,
                commands: res.data.commands || []
              }))
              .catch(() => ({
                device_id: device.device_id,
                commands: []
              }))
          )
        );
        return allCommands.flatMap(d => 
          d.commands.map(cmd => ({ ...cmd, device_id: d.device_id }))
        );
      } else {
        const res = await pollCommands(selectedDevice);
        return (res.data.commands || []).map(cmd => ({
          ...cmd,
          device_id: selectedDevice
        }));
      }
    },
    enabled: devices.length > 0,
    refetchInterval: autoRefresh ? 10000 : false, // 10 seconds
    staleTime: 5000
  });

  const commands = commandsData || [];

  // Cancel command mutation
  const cancelMutation = useMutation({
    mutationFn: async (commandId) => {
      // TODO: Implement cancel command API
      throw new Error('Cancel command not yet implemented');
    },
    onSuccess: () => {
      queryClient.invalidateQueries(['pendingCommands']);
    }
  });

  const handleCancelCommand = (commandId) => {
    if (window.confirm('Are you sure you want to cancel this command?')) {
      cancelMutation.mutate(commandId);
    }
  };

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
  };

  const handleManualRefresh = () => {
    refetch();
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'pending':
        return 'warning';
      case 'executing':
        return 'info';
      case 'sent':
        return 'primary';
      default:
        return 'default';
    }
  };

  const formatTimestamp = (timestamp) => {
    return new Date(timestamp * 1000).toLocaleString();
  };

  const getCommandDescription = (command) => {
    if (command.command_type === 'write_register') {
      return `Write Register ${command.register_address || '?'}: ${command.value || '?'}`;
    }
    return command.command_type || 'Unknown';
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        {/* Header */}
        <Box sx={{ mb: 3, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Stack direction="row" spacing={2} alignItems="center">
            <Typography variant="h5">
              Command Queue
            </Typography>
            <Chip 
              icon={<TimerIcon />}
              label={autoRefresh ? 'Auto-refresh: 10s' : 'Auto-refresh: OFF'}
              color={autoRefresh ? 'success' : 'default'}
              size="small"
            />
          </Stack>
          
          <Stack direction="row" spacing={2}>
            {/* Device Filter */}
            <FormControl size="small" sx={{ minWidth: 200 }}>
              <InputLabel>Device</InputLabel>
              <Select
                value={selectedDevice}
                label="Device"
                onChange={handleDeviceChange}
              >
                <MenuItem value="all">All Devices</MenuItem>
                {devices.map((device) => (
                  <MenuItem key={device.device_id} value={device.device_id}>
                    {device.device_name || device.device_id}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>

            {/* Manual Refresh */}
            <Tooltip title="Refresh now">
              <IconButton onClick={handleManualRefresh} color="primary">
                <RefreshIcon />
              </IconButton>
            </Tooltip>
          </Stack>
        </Box>

        {/* Loading State */}
        {isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading commands: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Empty State */}
        {!isLoading && !isError && commands.length === 0 && (
          <Alert severity="info">
            No pending commands in the queue
          </Alert>
        )}

        {/* Commands Table */}
        {!isLoading && !isError && commands.length > 0 && (
          <TableContainer>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell>Timestamp</TableCell>
                  <TableCell>Device</TableCell>
                  <TableCell>Command ID</TableCell>
                  <TableCell>Command</TableCell>
                  <TableCell>Status</TableCell>
                  <TableCell>Priority</TableCell>
                  <TableCell align="center">Actions</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {commands.map((command) => (
                  <TableRow key={command.command_id}>
                    <TableCell>
                      {formatTimestamp(command.timestamp)}
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" fontWeight="medium">
                        {command.device_id}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" sx={{ fontFamily: 'monospace', fontSize: '0.85rem' }}>
                        {command.command_id?.substring(0, 8)}...
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2">
                        {getCommandDescription(command)}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      <Chip 
                        label={command.status || 'pending'}
                        color={getStatusColor(command.status)}
                        size="small"
                      />
                    </TableCell>
                    <TableCell>
                      <Chip 
                        label={command.priority || 'normal'}
                        variant="outlined"
                        size="small"
                      />
                    </TableCell>
                    <TableCell align="center">
                      <Tooltip title="Cancel command">
                        <IconButton
                          size="small"
                          color="error"
                          onClick={() => handleCancelCommand(command.command_id)}
                          disabled={command.status === 'executing'}
                        >
                          <CancelIcon />
                        </IconButton>
                      </Tooltip>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        )}

        {/* Footer Info */}
        <Box sx={{ mt: 2, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <Typography variant="body2" color="text.secondary">
            Total: {commands.length} pending command{commands.length !== 1 ? 's' : ''}
          </Typography>
          <Typography variant="caption" color="text.secondary">
            Last updated: {new Date().toLocaleTimeString()}
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default CommandQueue;
