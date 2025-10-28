/**
 * DiagnosticsSummary Component
 * 
 * Displays device health metrics and system health information
 * Features:
 * - Device health indicators
 * - System uptime
 * - API response time
 * - Recent diagnostic data
 * - Alert indicators
 */

import React from 'react';
import {
  Box,
  Paper,
  Typography,
  Grid,
  Chip,
  Alert,
  CircularProgress,
  Divider,
  Stack
} from '@mui/material';
import {
  CheckCircle as HealthyIcon,
  Warning as WarningIcon,
  Error as ErrorIcon,
  Speed as SpeedIcon,
  Timer as UptimeIcon,
  Memory as MemoryIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getDiagnosticsSummary, getSystemHealth } from '../../api/diagnostics';
import StatisticsCard from '../common/StatisticsCard';

const DiagnosticsSummary = ({ deviceId = null }) => {
  // Fetch diagnostics summary
  const {
    data: summaryData,
    isLoading: summaryLoading,
    isError: summaryError
  } = useQuery({
    queryKey: ['diagnosticsSummary', deviceId],
    queryFn: () => getDiagnosticsSummary(deviceId),
    refetchInterval: 30000 // Refresh every 30 seconds
  });

  // Fetch system health
  const {
    data: healthData,
    isLoading: healthLoading,
    isError: healthError
  } = useQuery({
    queryKey: ['systemHealth'],
    queryFn: getSystemHealth,
    refetchInterval: 10000 // Refresh every 10 seconds
  });

  const summary = summaryData?.data?.summary || {};
  const health = healthData?.data || {};

  const getHealthStatus = (summary) => {
    if (!summary || Object.keys(summary).length === 0) {
      return { status: 'unknown', color: 'default', icon: <WarningIcon /> };
    }
    
    const errorCount = summary.error_count || 0;
    const warningCount = summary.warning_count || 0;
    
    if (errorCount > 0) {
      return { status: 'critical', color: 'error', icon: <ErrorIcon /> };
    }
    if (warningCount > 0) {
      return { status: 'warning', color: 'warning', icon: <WarningIcon /> };
    }
    return { status: 'healthy', color: 'success', icon: <HealthyIcon /> };
  };

  const healthStatus = getHealthStatus(summary);

  const formatUptime = (seconds) => {
    if (!seconds) return 'N/A';
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) return `${days}d ${hours}h`;
    if (hours > 0) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
  };

  if (summaryLoading || healthLoading) {
    return (
      <Box sx={{ display: 'flex', justifyContent: 'center', p: 4 }}>
        <CircularProgress />
      </Box>
    );
  }

  if (summaryError || healthError) {
    return (
      <Alert severity="error" sx={{ mb: 2 }}>
        Error loading diagnostics summary
      </Alert>
    );
  }

  return (
    <Box>
      {/* Overall Health Status */}
      <Paper sx={{ p: 3, mb: 3 }}>
        <Box sx={{ display: 'flex', alignItems: 'center', mb: 2 }}>
          <Typography variant="h6" sx={{ flexGrow: 1 }}>
            {deviceId ? `Device Health: ${deviceId}` : 'System Health'}
          </Typography>
          <Chip
            icon={healthStatus.icon}
            label={healthStatus.status.toUpperCase()}
            color={healthStatus.color}
            sx={{ fontWeight: 'bold' }}
          />
        </Box>

        <Divider sx={{ my: 2 }} />

        {/* Health Metrics */}
        <Grid container spacing={2}>
          <Grid item xs={12} sm={6} md={3}>
            <Box>
              <Typography variant="body2" color="text.secondary">
                Total Records
              </Typography>
              <Typography variant="h6" fontWeight="bold">
                {summary.total_records || 0}
              </Typography>
            </Box>
          </Grid>

          <Grid item xs={12} sm={6} md={3}>
            <Box>
              <Typography variant="body2" color="text.secondary">
                Errors
              </Typography>
              <Typography variant="h6" fontWeight="bold" color="error.main">
                {summary.error_count || 0}
              </Typography>
            </Box>
          </Grid>

          <Grid item xs={12} sm={6} md={3}>
            <Box>
              <Typography variant="body2" color="text.secondary">
                Warnings
              </Typography>
              <Typography variant="h6" fontWeight="bold" color="warning.main">
                {summary.warning_count || 0}
              </Typography>
            </Box>
          </Grid>

          <Grid item xs={12} sm={6} md={3}>
            <Box>
              <Typography variant="body2" color="text.secondary">
                Last Update
              </Typography>
              <Typography variant="h6" fontWeight="bold">
                {summary.last_update ? new Date(summary.last_update).toLocaleTimeString() : 'N/A'}
              </Typography>
            </Box>
          </Grid>
        </Grid>
      </Paper>

      {/* System Metrics */}
      <Typography variant="h6" gutterBottom>
        System Metrics
      </Typography>
      
      <Grid container spacing={3}>
        <Grid item xs={12} sm={6} md={4}>
          <StatisticsCard
            title="System Uptime"
            value={formatUptime(health.uptime)}
            icon={UptimeIcon}
            color="primary"
            subtitle={health.start_time ? `Started: ${new Date(health.start_time).toLocaleString()}` : ''}
          />
        </Grid>

        <Grid item xs={12} sm={6} md={4}>
          <StatisticsCard
            title="API Response Time"
            value={health.response_time || 'N/A'}
            unit="ms"
            icon={SpeedIcon}
            color="info"
            subtitle="Average response time"
          />
        </Grid>

        <Grid item xs={12} sm={6} md={4}>
          <StatisticsCard
            title="Active Devices"
            value={health.active_devices || 0}
            icon={MemoryIcon}
            color="success"
            subtitle={`Total: ${health.total_devices || 0} devices`}
          />
        </Grid>
      </Grid>

      {/* Recent Issues */}
      {summary.recent_issues && summary.recent_issues.length > 0 && (
        <Box sx={{ mt: 3 }}>
          <Typography variant="h6" gutterBottom>
            Recent Issues
          </Typography>
          <Stack spacing={1}>
            {summary.recent_issues.slice(0, 5).map((issue, index) => (
              <Alert
                key={index}
                severity={issue.severity || 'info'}
                sx={{ fontSize: '0.875rem' }}
              >
                <strong>{issue.timestamp}:</strong> {issue.message}
              </Alert>
            ))}
          </Stack>
        </Box>
      )}
    </Box>
  );
};

export default DiagnosticsSummary;
