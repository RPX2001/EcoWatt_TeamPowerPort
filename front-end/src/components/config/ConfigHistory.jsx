/**
 * ConfigHistory Component
 * 
 * Displays configuration change history
 * Features:
 * - Timeline view of config changes
 * - Diff comparison between versions
 * - Filter by device
 * - Expandable details showing what changed
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
  Grid,
  Divider
} from '@mui/material';
import {
  ExpandMore as ExpandMoreIcon,
  ExpandLess as ExpandLessIcon,
  History as HistoryIcon,
  Compare as CompareIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getConfigHistory } from '../../api/config';
import { getDevices } from '../../api/devices';

const ConfigHistory = () => {
  const [selectedDevice, setSelectedDevice] = useState('');
  const [expandedRows, setExpandedRows] = useState(new Set());

  // Fetch devices
  const { 
    data: devicesData, 
    isLoading: devicesLoading,
    isError: devicesError 
  } = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    staleTime: 30000
  });

  const devices = devicesData?.data?.devices || [];

  // Auto-select first device
  React.useEffect(() => {
    if (devices.length > 0 && !selectedDevice) {
      setSelectedDevice(devices[0].device_id);
    }
  }, [devices, selectedDevice]);

  // Fetch config history
  const {
    data: historyData,
    isLoading,
    isError,
    error
  } = useQuery({
    queryKey: ['configHistory', selectedDevice],
    queryFn: () => getConfigHistory(selectedDevice),
    enabled: !!selectedDevice,
    staleTime: 10000
  });

  const history = historyData?.data?.history || [];

  const handleDeviceChange = (event) => {
    setSelectedDevice(event.target.value);
  };

  const toggleRowExpand = (index) => {
    setExpandedRows(prev => {
      const newSet = new Set(prev);
      if (newSet.has(index)) {
        newSet.delete(index);
      } else {
        newSet.add(index);
      }
      return newSet;
    });
  };

  const formatTimestamp = (timestamp) => {
    return new Date(timestamp * 1000).toLocaleString();
  };

  const getChangedFields = (current, previous) => {
    if (!previous) return Object.keys(current);
    
    const changed = [];
    for (const key in current) {
      if (JSON.stringify(current[key]) !== JSON.stringify(previous[key])) {
        changed.push(key);
      }
    }
    return changed;
  };

  const renderConfigDiff = (current, previous) => {
    if (!previous) {
      return (
        <Alert severity="info" sx={{ mb: 2 }}>
          Initial configuration
        </Alert>
      );
    }

    const changedFields = getChangedFields(current, previous);

    if (changedFields.length === 0) {
      return (
        <Alert severity="info" sx={{ mb: 2 }}>
          No changes detected
        </Alert>
      );
    }

    return (
      <Box>
        <Typography variant="subtitle2" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
          <CompareIcon fontSize="small" />
          Changed Fields ({changedFields.length})
        </Typography>
        
        <Grid container spacing={2} sx={{ mt: 1 }}>
          {changedFields.map((field) => (
            <React.Fragment key={field}>
              <Grid item xs={12}>
                <Box sx={{ p: 2, bgcolor: 'grey.50', borderRadius: 1 }}>
                  <Typography variant="body2" fontWeight="bold" gutterBottom>
                    {field}
                  </Typography>
                  
                  <Grid container spacing={2}>
                    <Grid item xs={6}>
                      <Box sx={{ p: 1, bgcolor: 'error.50', borderRadius: 1, border: '1px solid', borderColor: 'error.light' }}>
                        <Typography variant="caption" color="error.dark" fontWeight="bold">
                          Previous:
                        </Typography>
                        <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5 }}>
                          {JSON.stringify(previous[field], null, 2)}
                        </Typography>
                      </Box>
                    </Grid>
                    
                    <Grid item xs={6}>
                      <Box sx={{ p: 1, bgcolor: 'success.50', borderRadius: 1, border: '1px solid', borderColor: 'success.light' }}>
                        <Typography variant="caption" color="success.dark" fontWeight="bold">
                          Current:
                        </Typography>
                        <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5 }}>
                          {JSON.stringify(current[field], null, 2)}
                        </Typography>
                      </Box>
                    </Grid>
                  </Grid>
                </Box>
              </Grid>
            </React.Fragment>
          ))}
        </Grid>
      </Box>
    );
  };

  const renderConfigDetails = (config) => {
    return (
      <Box sx={{ display: 'grid', gridTemplateColumns: '200px 1fr', gap: 1 }}>
        {/* Timing Parameters */}
        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Sampling Interval:
        </Typography>
        <Typography variant="body2">
          {config.sampling_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Upload Interval:
        </Typography>
        <Typography variant="body2">
          {config.upload_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Firmware Check Interval:
        </Typography>
        <Typography variant="body2">
          {config.firmware_check_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Command Poll Interval:
        </Typography>
        <Typography variant="body2">
          {config.command_poll_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Config Poll Interval:
        </Typography>
        <Typography variant="body2">
          {config.config_poll_interval || 'N/A'} seconds
        </Typography>

        {/* Modbus Registers */}
        {config.modbus_registers && config.modbus_registers.length > 0 && (
          <>
            <Typography variant="body2" color="text.secondary" fontWeight="bold" sx={{ mt: 1 }}>
              Modbus Registers:
            </Typography>
            <Box sx={{ mt: 1 }}>
              {config.modbus_registers.map((reg, idx) => (
                <Chip 
                  key={idx}
                  label={`Reg ${reg}`}
                  size="small"
                  sx={{ mr: 0.5, mb: 0.5 }}
                />
              ))}
            </Box>
          </>
        )}
      </Box>
    );
  };

  return (
    <Box sx={{ p: 3 }}>
      <Paper sx={{ p: 3 }}>
        {/* Header */}
        <Box sx={{ mb: 3 }}>
          <Stack direction="row" spacing={2} alignItems="center" sx={{ mb: 2 }}>
            <HistoryIcon color="primary" />
            <Typography variant="h5">
              Configuration History
            </Typography>
          </Stack>
          
          {/* Loading Devices */}
          {devicesLoading && (
            <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
              <CircularProgress size={20} />
              <Typography variant="body2" color="text.secondary">
                Loading devices...
              </Typography>
            </Box>
          )}

          {/* Device Error */}
          {devicesError && (
            <Alert severity="error" sx={{ mb: 2 }}>
              Error loading devices. Please refresh the page.
            </Alert>
          )}

          {/* No Devices */}
          {!devicesLoading && !devicesError && devices.length === 0 && (
            <Alert severity="warning" sx={{ mb: 2 }}>
              No devices available. Please add a device first.
            </Alert>
          )}

          {/* Device Filter */}
          {!devicesLoading && !devicesError && devices.length > 0 && (
            <FormControl size="small" sx={{ minWidth: 250 }}>
              <InputLabel>Device</InputLabel>
              <Select
                value={selectedDevice}
                label="Device"
                onChange={handleDeviceChange}
              >
                {devices.map((device) => (
                  <MenuItem key={device.device_id} value={device.device_id}>
                    {device.device_name || device.device_id}
                  </MenuItem>
                ))}
              </Select>
            </FormControl>
          )}
        </Box>

        {/* Loading State */}
        {selectedDevice && isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {selectedDevice && isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading configuration history: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Empty State */}
        {selectedDevice && !isLoading && !isError && history.length === 0 && (
          <Alert severity="info">
            No configuration history found for this device
          </Alert>
        )}

        {/* History Table */}
        {selectedDevice && !isLoading && !isError && history.length > 0 && (
          <TableContainer>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell width={50}></TableCell>
                  <TableCell>Timestamp</TableCell>
                  <TableCell>Changes</TableCell>
                  <TableCell>Status</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {history.map((entry, index) => {
                  const previousEntry = index < history.length - 1 ? history[index + 1] : null;
                  const changedFields = getChangedFields(
                    entry.config,
                    previousEntry?.config
                  );

                  return (
                    <React.Fragment key={index}>
                      <TableRow hover>
                        <TableCell>
                          <IconButton
                            size="small"
                            onClick={() => toggleRowExpand(index)}
                          >
                            {expandedRows.has(index) ? (
                              <ExpandLessIcon />
                            ) : (
                              <ExpandMoreIcon />
                            )}
                          </IconButton>
                        </TableCell>
                        <TableCell>
                          {formatTimestamp(entry.timestamp)}
                        </TableCell>
                        <TableCell>
                          {previousEntry ? (
                            <Typography variant="body2">
                              {changedFields.length} field{changedFields.length !== 1 ? 's' : ''} changed
                            </Typography>
                          ) : (
                            <Chip label="Initial" size="small" color="primary" />
                          )}
                        </TableCell>
                        <TableCell>
                          <Chip
                            label={entry.status || 'applied'}
                            color="success"
                            size="small"
                          />
                        </TableCell>
                      </TableRow>

                      {/* Expanded Details Row */}
                      <TableRow>
                        <TableCell colSpan={4} sx={{ p: 0, border: 0 }}>
                          <Collapse in={expandedRows.has(index)}>
                            <Box sx={{ p: 3, bgcolor: 'grey.50' }}>
                              {/* Diff View */}
                              <Box sx={{ mb: 3 }}>
                                <Typography variant="h6" gutterBottom>
                                  Changes
                                </Typography>
                                {renderConfigDiff(entry.config, previousEntry?.config)}
                              </Box>

                              <Divider sx={{ my: 2 }} />

                              {/* Full Config */}
                              <Box>
                                <Typography variant="h6" gutterBottom>
                                  Full Configuration
                                </Typography>
                                {renderConfigDetails(entry.config)}
                              </Box>
                            </Box>
                          </Collapse>
                        </TableCell>
                      </TableRow>
                    </React.Fragment>
                  );
                })}
              </TableBody>
            </Table>
          </TableContainer>
        )}

        {/* Footer */}
        <Box sx={{ mt: 2 }}>
          <Typography variant="body2" color="text.secondary">
            Showing {history.length} configuration change{history.length !== 1 ? 's' : ''}
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default ConfigHistory;
