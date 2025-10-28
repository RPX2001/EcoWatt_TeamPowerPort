/**
 * CommandHistory Component
 * 
 * Displays command execution history
 * Features:
 * - Pagination
 * - Filter by status (all, completed, failed, executing, pending)
 * - Filter by device
 * - Status color indicators
 * - Detailed result view
 */

import React, { useState } from 'react';
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
  TablePagination,
  Chip,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Alert,
  CircularProgress,
  Stack,
  IconButton,
  Collapse,
  Tooltip
} from '@mui/material';
import {
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  HourglassEmpty as PendingIcon,
  PlayArrow as ExecutingIcon,
  ExpandMore as ExpandMoreIcon,
  ExpandLess as ExpandLessIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getCommandHistory } from '../../api/commands';
import { getDevices } from '../../api/devices';

const CommandHistory = () => {
  const [selectedDevice, setSelectedDevice] = useState('all');
  const [statusFilter, setStatusFilter] = useState('all');
  const [page, setPage] = useState(0);
  const [rowsPerPage, setRowsPerPage] = useState(10);
  const [expandedRows, setExpandedRows] = useState(new Set());

  // Fetch devices for filter
  const { data: devicesData } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Fetch command history
  const {
    data: historyData,
    isLoading,
    isError,
    error
  } = useQuery({
    queryKey: ['commandHistory', selectedDevice],
    queryFn: async () => {
      if (selectedDevice === 'all') {
        // Fetch for all devices
        const allHistory = await Promise.all(
          devices.map(device =>
            getCommandHistory(device.device_id)
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
        return allHistory.flatMap(d =>
          d.commands.map(cmd => ({ ...cmd, device_id: d.device_id }))
        );
      } else {
        const res = await getCommandHistory(selectedDevice);
        return (res.data.commands || []).map(cmd => ({
          ...cmd,
          device_id: selectedDevice
        }));
      }
    },
    enabled: devices.length > 0,
    staleTime: 10000
  });

  const allCommands = historyData || [];

  // Filter by status
  const filteredCommands = statusFilter === 'all'
    ? allCommands
    : allCommands.filter(cmd => cmd.status === statusFilter);

  // Paginate
  const paginatedCommands = filteredCommands.slice(
    page * rowsPerPage,
    page * rowsPerPage + rowsPerPage
  );

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
    setPage(0);
  };

  const handleStatusChange = (event) => {
    setStatusFilter(event.target.value);
    setPage(0);
  };

  const handleChangePage = (event, newPage) => {
    setPage(newPage);
  };

  const handleChangeRowsPerPage = (event) => {
    setRowsPerPage(parseInt(event.target.value, 10));
    setPage(0);
  };

  const toggleRowExpand = (commandId) => {
    setExpandedRows(prev => {
      const newSet = new Set(prev);
      if (newSet.has(commandId)) {
        newSet.delete(commandId);
      } else {
        newSet.add(commandId);
      }
      return newSet;
    });
  };

  const getStatusIcon = (status) => {
    switch (status) {
      case 'completed':
      case 'success':
        return <SuccessIcon fontSize="small" color="success" />;
      case 'failed':
      case 'error':
        return <ErrorIcon fontSize="small" color="error" />;
      case 'executing':
        return <ExecutingIcon fontSize="small" color="info" />;
      case 'pending':
        return <PendingIcon fontSize="small" color="warning" />;
      default:
        return null;
    }
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'completed':
      case 'success':
        return 'success';
      case 'failed':
      case 'error':
        return 'error';
      case 'executing':
        return 'info';
      case 'pending':
        return 'warning';
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
    <Box sx={{ p: 3 }}>
      <Paper sx={{ p: 3 }}>
        {/* Header */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="h5" gutterBottom>
            Command History
          </Typography>
          
          <Stack direction="row" spacing={2} sx={{ mt: 2 }}>
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

            {/* Status Filter */}
            <FormControl size="small" sx={{ minWidth: 150 }}>
              <InputLabel>Status</InputLabel>
              <Select
                value={statusFilter}
                label="Status"
                onChange={handleStatusChange}
              >
                <MenuItem value="all">All Status</MenuItem>
                <MenuItem value="completed">Completed</MenuItem>
                <MenuItem value="failed">Failed</MenuItem>
                <MenuItem value="executing">Executing</MenuItem>
                <MenuItem value="pending">Pending</MenuItem>
              </Select>
            </FormControl>
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
            Error loading command history: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Empty State */}
        {!isLoading && !isError && filteredCommands.length === 0 && (
          <Alert severity="info">
            No command history found
          </Alert>
        )}

        {/* History Table */}
        {!isLoading && !isError && filteredCommands.length > 0 && (
          <>
            <TableContainer>
              <Table>
                <TableHead>
                  <TableRow>
                    <TableCell width={50}></TableCell>
                    <TableCell>Timestamp</TableCell>
                    <TableCell>Device</TableCell>
                    <TableCell>Command</TableCell>
                    <TableCell>Status</TableCell>
                    <TableCell>Result</TableCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {paginatedCommands.map((command) => (
                    <React.Fragment key={command.command_id}>
                      <TableRow hover>
                        <TableCell>
                          <IconButton
                            size="small"
                            onClick={() => toggleRowExpand(command.command_id)}
                          >
                            {expandedRows.has(command.command_id) ? (
                              <ExpandLessIcon />
                            ) : (
                              <ExpandMoreIcon />
                            )}
                          </IconButton>
                        </TableCell>
                        <TableCell>
                          {formatTimestamp(command.timestamp)}
                        </TableCell>
                        <TableCell>
                          <Typography variant="body2" fontWeight="medium">
                            {command.device_id}
                          </Typography>
                        </TableCell>
                        <TableCell>
                          <Typography variant="body2">
                            {getCommandDescription(command)}
                          </Typography>
                        </TableCell>
                        <TableCell>
                          <Chip
                            icon={getStatusIcon(command.status)}
                            label={command.status || 'unknown'}
                            color={getStatusColor(command.status)}
                            size="small"
                          />
                        </TableCell>
                        <TableCell>
                          <Typography
                            variant="body2"
                            sx={{
                              maxWidth: 200,
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              whiteSpace: 'nowrap'
                            }}
                          >
                            {command.result || '-'}
                          </Typography>
                        </TableCell>
                      </TableRow>
                      
                      {/* Expanded Details Row */}
                      <TableRow>
                        <TableCell colSpan={6} sx={{ p: 0, border: 0 }}>
                          <Collapse in={expandedRows.has(command.command_id)}>
                            <Box sx={{ p: 2, bgcolor: 'grey.50' }}>
                              <Typography variant="subtitle2" gutterBottom>
                                Command Details
                              </Typography>
                              <Box sx={{ display: 'grid', gridTemplateColumns: '150px 1fr', gap: 1 }}>
                                <Typography variant="body2" color="text.secondary">
                                  Command ID:
                                </Typography>
                                <Typography variant="body2" sx={{ fontFamily: 'monospace', fontSize: '0.85rem' }}>
                                  {command.command_id}
                                </Typography>
                                
                                <Typography variant="body2" color="text.secondary">
                                  Command Type:
                                </Typography>
                                <Typography variant="body2">
                                  {command.command_type}
                                </Typography>
                                
                                {command.register_address !== undefined && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Register Address:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.register_address}
                                    </Typography>
                                  </>
                                )}
                                
                                {command.value !== undefined && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Value:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.value}
                                    </Typography>
                                  </>
                                )}
                                
                                <Typography variant="body2" color="text.secondary">
                                  Priority:
                                </Typography>
                                <Typography variant="body2">
                                  {command.priority || 'normal'}
                                </Typography>
                                
                                {command.result && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Result:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.result}
                                    </Typography>
                                  </>
                                )}
                                
                                {command.error && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Error:
                                    </Typography>
                                    <Typography variant="body2" color="error.main">
                                      {command.error}
                                    </Typography>
                                  </>
                                )}
                              </Box>
                            </Box>
                          </Collapse>
                        </TableCell>
                      </TableRow>
                    </React.Fragment>
                  ))}
                </TableBody>
              </Table>
            </TableContainer>

            {/* Pagination */}
            <TablePagination
              component="div"
              count={filteredCommands.length}
              page={page}
              onPageChange={handleChangePage}
              rowsPerPage={rowsPerPage}
              onRowsPerPageChange={handleChangeRowsPerPage}
              rowsPerPageOptions={[5, 10, 25, 50]}
            />
          </>
        )}
      </Paper>
    </Box>
  );
};

export default CommandHistory;
