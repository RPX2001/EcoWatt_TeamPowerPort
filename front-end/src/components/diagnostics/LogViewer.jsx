/**
 * LogViewer Component
 * 
 * Displays diagnostic logs with filtering and search
 * Features:
 * - Scrollable log display
 * - Color coding by severity (INFO, WARNING, ERROR)
 * - Auto-scroll to bottom toggle
 * - Search functionality
 * - Export logs to CSV/JSON
 * - Real-time updates
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Box,
  Paper,
  Typography,
  List,
  ListItem,
  ListItemText,
  CircularProgress,
  Alert,
  Button,
  Switch,
  FormControlLabel,
  Stack,
  Divider,
  Chip
} from '@mui/material';
import {
  Refresh as RefreshIcon,
  Download as DownloadIcon
} from '@mui/icons-material';
import { useQuery, useQueryClient } from '@tanstack/react-query';
import { getDeviceLogs } from '../../api/devices';
import LogFilters from './LogFilters';

const LogViewer = ({ deviceId = null }) => {
  const [filters, setFilters] = useState({
    type: 'all',
    search: ''
  });
  const [autoScroll, setAutoScroll] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(false);
  const logEndRef = useRef(null);
  const queryClient = useQueryClient();

  // Fetch device logs from database
  const {
    data: logsData,
    isLoading,
    isError,
    refetch
  } = useQuery({
    queryKey: ['deviceLogs', deviceId],
    queryFn: () => {
      if (!deviceId) {
        throw new Error('Device ID is required');
      }
      return getDeviceLogs(deviceId, 200);
    },
    enabled: !!deviceId,
    refetchInterval: autoRefresh ? 10000 : false,
    staleTime: 5000
  });

  // Remove clear mutation since we don't want to delete sensor data
  // const clearMutation = useMutation({...});

  // Auto-scroll to bottom
  useEffect(() => {
    if (autoScroll && logEndRef.current) {
      logEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logsData, autoScroll]);

  const handleFilterChange = (newFilters) => {
    setFilters(newFilters);
  };

  const handleClearFilters = () => {
    setFilters({
      type: 'all',
      search: ''
    });
  };

  // Remove clear logs handler since we don't want to delete sensor data
  // const handleClearLogs = () => {...};

  const handleExport = (format) => {
    const logs = getFilteredLogs();
    
    if (format === 'json') {
      const dataStr = JSON.stringify(logs, null, 2);
      const dataBlob = new Blob([dataStr], { type: 'application/json' });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `device_logs_${deviceId}_${new Date().toISOString()}.json`;
      link.click();
    } else if (format === 'csv') {
      const headers = ['Timestamp', 'Level', 'Type', 'Message', 'Details'];
      const csvRows = [headers.join(',')];
      
      logs.forEach(log => {
        const detailsStr = log.details ? JSON.stringify(log.details).replace(/"/g, '""') : '';
        const row = [
          log.timestamp,
          log.level || 'INFO',
          log.type || 'ACTIVITY',
          `"${(log.message || '').replace(/"/g, '""')}"`,
          `"${detailsStr}"`
        ];
        csvRows.push(row.join(','));
      });
      
      const csvStr = csvRows.join('\n');
      const dataBlob = new Blob([csvStr], { type: 'text/csv' });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `device_logs_${deviceId}_${new Date().toISOString()}.csv`;
      link.click();
    }
  };

  const getLogColor = (level) => {
    switch (level?.toLowerCase()) {
      case 'error':
        return 'error.main';
      case 'warning':
        return 'warning.main';
      case 'success':
        return 'success.main';
      case 'info':
        return 'info.main';
      case 'debug':
        return 'text.secondary';
      default:
        return 'text.primary';
    }
  };

  const getLogIcon = (type, level) => {
    // First check type for specific icons
    switch (type?.toUpperCase()) {
      case 'DATA_UPLOAD':
        return '📊';
      case 'COMMAND':
        return '⚡';
      case 'CONFIG_CHANGE':
        return '⚙️';
      case 'OTA_UPDATE':
        return '🔄';
      case 'FAULT_INJECTION':
        return '🧪';
      case 'FAULT_RECOVERY':
        return '🔧';
      default:
        break;
    }
    
    // Fallback to level-based icons
    switch (level?.toLowerCase()) {
      case 'error':
        return '❌';
      case 'warning':
        return '⚠️';
      case 'success':
        return '✅';
      case 'info':
        return 'ℹ️';
      case 'debug':
        return '🔍';
      default:
        return '📝';
    }
  };

  const getTypeChipColor = (type) => {
    switch (type?.toUpperCase()) {
      case 'DATA_UPLOAD':
        return 'primary';
      case 'COMMAND':
        return 'secondary';
      case 'CONFIG_CHANGE':
        return 'info';
      case 'OTA_UPDATE':
        return 'warning';
      case 'FAULT_INJECTION':
        return 'error';
      case 'FAULT_RECOVERY':
        return 'success';
      default:
        return 'default';
    }
  };

  const getFilteredLogs = () => {
    const logs = logsData?.data?.logs || [];

    // Apply filters
    let filtered = logs;

    // Filter by activity type
    if (filters.type && filters.type !== 'all') {
      filtered = filtered.filter(log => 
        log.type === filters.type
      );
    }

    // Filter by search text
    if (filters.search) {
      const searchLower = filters.search.toLowerCase();
      filtered = filtered.filter(log => 
        (log.message || '').toLowerCase().includes(searchLower) ||
        (log.type || '').toLowerCase().includes(searchLower) ||
        (log.details?.registers || []).join(' ').toLowerCase().includes(searchLower)
      );
    }

    return filtered;
  };

  const filteredLogs = getFilteredLogs();

  return (
    <Box>
      {/* Filters */}
      <LogFilters
        filters={filters}
        onFilterChange={handleFilterChange}
        onClearFilters={handleClearFilters}
      />

      {/* Controls */}
      <Paper sx={{ p: 2, mb: 2 }}>
        <Stack direction="row" spacing={2} alignItems="center" flexWrap="wrap">
          <FormControlLabel
            control={
              <Switch
                checked={autoScroll}
                onChange={(e) => setAutoScroll(e.target.checked)}
              />
            }
            label="Auto-scroll"
          />
          
          <FormControlLabel
            control={
              <Switch
                checked={autoRefresh}
                onChange={(e) => setAutoRefresh(e.target.checked)}
              />
            }
            label="Auto-refresh (10s)"
          />

          <Divider orientation="vertical" flexItem />

          <Button
            size="small"
            startIcon={<RefreshIcon />}
            onClick={() => refetch()}
            disabled={isLoading}
          >
            Refresh
          </Button>

          <Button
            size="small"
            startIcon={<DownloadIcon />}
            onClick={() => handleExport('json')}
            disabled={filteredLogs.length === 0}
          >
            Export JSON
          </Button>

          <Button
            size="small"
            startIcon={<DownloadIcon />}
            onClick={() => handleExport('csv')}
            disabled={filteredLogs.length === 0}
          >
            Export CSV
          </Button>

          <Box sx={{ flexGrow: 1 }} />

          <Chip 
            label={`${filteredLogs.length} logs`} 
            size="small" 
            color="primary"
          />
        </Stack>
      </Paper>

      {/* Log Display */}
      <Paper sx={{ p: 0, maxHeight: 600, overflow: 'auto' }}>
        {!deviceId && (
          <Alert severity="info" sx={{ m: 2 }}>
            Please select a device to view diagnostic logs.
          </Alert>
        )}

        {deviceId && isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', p: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {deviceId && isError && (
          <Alert severity="error" sx={{ m: 2 }}>
            Error loading diagnostics logs
          </Alert>
        )}

        {deviceId && !isLoading && !isError && filteredLogs.length === 0 && (
          <Alert severity="info" sx={{ m: 2 }}>
            No logs found. Adjust filters or wait for new diagnostic data.
          </Alert>
        )}

        {deviceId && !isLoading && !isError && filteredLogs.length > 0 && (
          <List sx={{ p: 0 }}>
            {filteredLogs.map((log, index) => (
              <React.Fragment key={index}>
                <ListItem
                  sx={{
                    borderLeft: `4px solid`,
                    borderLeftColor: getLogColor(log.level),
                    '&:hover': { bgcolor: 'action.hover' }
                  }}
                >
                  <ListItemText
                    primary={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, flexWrap: 'wrap' }}>
                        <Typography component="span" sx={{ fontSize: '1.2rem' }}>
                          {getLogIcon(log.type, log.level)}
                        </Typography>
                        <Chip 
                          label={log.type || 'ACTIVITY'} 
                          size="small"
                          color={getTypeChipColor(log.type)}
                          variant="outlined"
                          sx={{ fontWeight: 'bold' }}
                        />
                        <Chip 
                          label={log.level || 'INFO'} 
                          size="small"
                          sx={{ 
                            minWidth: 70,
                            bgcolor: getLogColor(log.level),
                            color: 'white',
                            fontWeight: 'bold'
                          }}
                        />
                        <Typography variant="body2" color="text.secondary">
                          {new Date(log.timestamp).toLocaleString()}
                        </Typography>
                      </Box>
                    }
                    secondary={
                      <Box sx={{ mt: 1 }}>
                        <Typography variant="body1" color="text.primary" sx={{ mb: 0.5 }}>
                          {log.message}
                        </Typography>
                        {log.details && Object.keys(log.details).length > 0 && (
                          <Box 
                            sx={{ 
                              mt: 1, 
                              p: 1, 
                              bgcolor: 'background.default',
                              borderRadius: 1,
                              fontFamily: 'monospace',
                              fontSize: '0.85rem'
                            }}
                          >
                            {/* Show relevant details based on activity type */}
                            {log.type === 'DATA_UPLOAD' && (
                              <Box>
                                <Typography variant="caption" color="text.secondary">
                                  Registers: {log.details.registers?.join(', ')}
                                </Typography>
                                {log.details.compression_method && (
                                  <>
                                    <br />
                                    <Typography variant="caption" color="text.secondary">
                                      Compression: {log.details.compression_method} 
                                      {log.details.compression_ratio && ` (${log.details.compression_ratio}x)`}
                                    </Typography>
                                  </>
                                )}
                              </Box>
                            )}
                            
                            {log.type === 'COMMAND' && (
                              <Box>
                                <Typography variant="caption" color="text.secondary">
                                  Type: {log.details.command_type}
                                </Typography>
                                {log.details.result && (
                                  <>
                                    <br />
                                    <Typography variant="caption" color="text.secondary">
                                      Result: {log.details.result}
                                    </Typography>
                                  </>
                                )}
                              </Box>
                            )}
                            
                            {log.type === 'OTA_UPDATE' && (
                              <Box>
                                <Typography variant="caption" color="text.secondary">
                                  Version: {log.details.from_version} → {log.details.to_version}
                                </Typography>
                                {log.details.progress !== undefined && (
                                  <>
                                    <br />
                                    <Typography variant="caption" color="text.secondary">
                                      Progress: {log.details.progress}%
                                    </Typography>
                                  </>
                                )}
                                {log.details.error && (
                                  <>
                                    <br />
                                    <Typography variant="caption" color="error">
                                      Error: {log.details.error}
                                    </Typography>
                                  </>
                                )}
                              </Box>
                            )}
                            
                            {log.type === 'CONFIG_CHANGE' && (
                              <Typography variant="caption" color="text.secondary">
                                Keys: {log.details.config_keys?.join(', ')}
                              </Typography>
                            )}
                            
                            {(log.type === 'FAULT_INJECTION' || log.type === 'FAULT_RECOVERY') && (
                              <Box>
                                <Typography variant="caption" color="text.secondary">
                                  Fault: {log.details.fault_type}
                                </Typography>
                                {log.details.recovery_action && (
                                  <>
                                    <br />
                                    <Typography variant="caption" color="text.secondary">
                                      Action: {log.details.recovery_action}
                                    </Typography>
                                  </>
                                )}
                              </Box>
                            )}
                          </Box>
                        )}
                      </Box>
                    }
                  />
                </ListItem>
                {index < filteredLogs.length - 1 && <Divider />}
              </React.Fragment>
            ))}
            <div ref={logEndRef} />
          </List>
        )}
      </Paper>
    </Box>
  );
};

export default LogViewer;
