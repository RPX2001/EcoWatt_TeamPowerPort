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

const CommandHistory = ({ deviceId }) => {
  const [statusFilter, setStatusFilter] = useState('all');
  const [page, setPage] = useState(0);
  const [rowsPerPage, setRowsPerPage] = useState(10);
  const [expandedRows, setExpandedRows] = useState(new Set());

  // Fetch command history
  const {
    data: historyData,
    isLoading,
    isError,
    error
  } = useQuery({
    queryKey: ['commandHistory', deviceId],
    queryFn: async () => {
      if (!deviceId) {
        return { commands: [] };
      }
      const res = await getCommandHistory(deviceId);
      return res.data;
    },
    enabled: !!deviceId,
    staleTime: 10000
  });

  const allCommands = historyData?.commands || [];

  // Filter by status
  const filteredCommands = statusFilter === 'all'
    ? allCommands
    : allCommands.filter(cmd => cmd.status === statusFilter);

  // Paginate
  const paginatedCommands = filteredCommands.slice(
    page * rowsPerPage,
    page * rowsPerPage + rowsPerPage
  );

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
    if (!timestamp) return 'N/A';
    // Backend sends ISO format timestamp already in Sri Lanka timezone
    const date = new Date(timestamp);
    if (isNaN(date.getTime())) return 'Invalid Date';
    // Display in local format (24-hour format)
    return date.toLocaleString('en-GB', { 
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    });
  };

  const getCommandDescription = (cmd) => {
    // Handle nested command object structure from database
    const command = cmd.command || cmd;
    
    if (command.action === 'write_register') {
      const register = command.target_register || command.register_address || '?';
      const value = command.value !== undefined ? command.value : '?';
      return `Write Register ${register}: ${value}`;
    }
    return command.action || command.command_type || 'Unknown';
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

        {/* No Device Selected */}
        {!deviceId && (
          <Alert severity="info">
            Please select a device to view command history.
          </Alert>
        )}

        {/* Loading State */}
        {deviceId && isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {deviceId && isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading command history: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Empty State */}
        {deviceId && !isLoading && !isError && filteredCommands.length === 0 && (
          <Alert severity="info">
            No command history found
          </Alert>
        )}

        {/* History Table */}
        {deviceId && !isLoading && !isError && filteredCommands.length > 0 && (
          <>
            <TableContainer>
              <Table>
                <TableHead>
                  <TableRow>
                    <TableCell width={50}></TableCell>
                    <TableCell>Timestamp</TableCell>
                    <TableCell>Command</TableCell>
                    <TableCell>Status</TableCell>
                    <TableCell>Result</TableCell>
                    <TableCell>Acknowledged</TableCell>
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
                          {formatTimestamp(command.created_at)}
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
                            {command.result 
                              ? (typeof command.result === 'object' 
                                  ? JSON.stringify(command.result) 
                                  : command.result)
                              : '-'}
                          </Typography>
                        </TableCell>
                        <TableCell>
                          {command.acknowledged_at ? (
                            <Chip
                              label={`✓ ${formatTimestamp(command.acknowledged_at)}`}
                              color="success"
                              size="small"
                              variant="outlined"
                            />
                          ) : (
                            <Chip
                              label="Pending"
                              color="warning"
                              size="small"
                              variant="outlined"
                            />
                          )}
                        </TableCell>
                      </TableRow>
                      
                      {/* Expanded Details Row */}
                      <TableRow>
                        <TableCell colSpan={7} sx={{ p: 0, border: 0 }}>
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
                                  Action:
                                </Typography>
                                <Typography variant="body2">
                                  {command.command?.action || 'N/A'}
                                </Typography>
                                
                                {command.command?.target_register && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Target Register:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.command.target_register}
                                    </Typography>
                                  </>
                                )}
                                
                                {command.command?.register_address !== undefined && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Register Address:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.command.register_address}
                                    </Typography>
                                  </>
                                )}
                                
                                {command.command?.value !== undefined && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Value:
                                    </Typography>
                                    <Typography variant="body2">
                                      {command.command.value}
                                    </Typography>
                                  </>
                                )}
                                
                                <Typography variant="body2" color="text.secondary">
                                  Created At:
                                </Typography>
                                <Typography variant="body2">
                                  {formatTimestamp(command.created_at)}
                                </Typography>
                                
                                {command.result && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Result:
                                    </Typography>
                                    <Typography variant="body2">
                                      {typeof command.result === 'object' 
                                        ? JSON.stringify(command.result, null, 2)
                                        : command.result}
                                    </Typography>
                                  </>
                                )}
                                
                                {command.error_msg && (
                                  <>
                                    <Typography variant="body2" color="text.secondary">
                                      Error:
                                    </Typography>
                                    <Typography variant="body2" color="error.main">
                                      {command.error_msg}
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
