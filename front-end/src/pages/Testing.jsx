/**
 * Testing Page
 * 
 * Comprehensive testing interface for the EcoWatt system
 * Features:
 * - Fault Injection Testing
 * - Security Testing
 */

import React, { useState } from 'react';
import {
  Box,
  Container,
  Paper,
  Tabs,
  Tab,
  Typography,
  Alert
} from '@mui/material';
import {
  BugReport as FaultIcon,
  Security as SecurityIcon
} from '@mui/icons-material';
import FaultInjection from '../components/testing/FaultInjection';
import SecurityTests from '../components/testing/SecurityTests';

// Tab panel component
function TabPanel({ children, value, index }) {
  return (
    <div
      role="tabpanel"
      hidden={value !== index}
      id={`testing-tabpanel-${index}`}
      aria-labelledby={`testing-tab-${index}`}
    >
      {value === index && <Box sx={{ py: 3 }}>{children}</Box>}
    </div>
  );
}

const Testing = () => {
  const [activeTab, setActiveTab] = useState(0);

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
  };

  return (
    <Container maxWidth="xl">
      <Box sx={{ py: 4 }}>
        <Typography variant="h4" gutterBottom fontWeight="bold">
          Testing & Quality Assurance
        </Typography>
        <Typography variant="body1" color="text.secondary" gutterBottom>
          Comprehensive testing tools for fault injection and security validation
        </Typography>

        <Paper sx={{ mt: 3 }}>
          <Tabs
            value={activeTab}
            onChange={handleTabChange}
            aria-label="testing tabs"
            sx={{
              borderBottom: 1,
              borderColor: 'divider',
              px: 2
            }}
          >
            <Tab
              icon={<FaultIcon />}
              iconPosition="start"
              label="Fault Injection"
              id="testing-tab-0"
              aria-controls="testing-tabpanel-0"
            />
            <Tab
              icon={<SecurityIcon />}
              iconPosition="start"
              label="Security Tests"
              id="testing-tab-1"
              aria-controls="testing-tabpanel-1"
            />
          </Tabs>

          <TabPanel value={activeTab} index={0}>
            <FaultInjection />
          </TabPanel>

          <TabPanel value={activeTab} index={1}>
            <SecurityTests />
          </TabPanel>
        </Paper>
      </Box>
    </Container>
  );
};

export default Testing;
