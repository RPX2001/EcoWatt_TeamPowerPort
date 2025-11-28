import { useState } from 'react';
import { ThemeProvider } from '@mui/material/styles';
import CssBaseline from '@mui/material/CssBaseline';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { BrowserRouter, Routes, Route } from 'react-router-dom';
import theme from './theme/theme';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Navbar from './components/common/Navbar';
import Sidebar from './components/common/Sidebar';
import Footer from './components/common/Footer';
import Dashboard from './pages/Dashboard';
import Configuration from './pages/Configuration';
import Commands from './pages/Commands';
import FOTA from './pages/FOTA';
import Logs from './pages/Logs';
// import Utilities from './pages/Utilities'; // Hidden - will add later if needed
import Testing from './pages/Testing';

// Create a client
const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      refetchOnWindowFocus: false,
      retry: 1,
      staleTime: 5 * 60 * 1000, // 5 minutes
    },
  },
});

function App() {
  const [sidebarOpen, setSidebarOpen] = useState(true);

  const handleMenuClick = () => {
    setSidebarOpen(!sidebarOpen);
  };

  const handleSidebarClose = () => {
    // Keep sidebar open for desktop, close behavior can be added for mobile later
  };

  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider theme={theme}>
        <CssBaseline />
        <BrowserRouter>
          <Box sx={{ display: 'flex', flexDirection: 'column', minHeight: '100vh' }}>
            <Navbar onMenuClick={handleMenuClick} />
            <Box sx={{ display: 'flex', flexGrow: 1, minHeight: 'calc(100vh - 64px)' }}>
              <Sidebar open={sidebarOpen} onClose={handleSidebarClose} />
              <Box
                component="main"
                sx={{
                  flexGrow: 1,
                  p: 2,
                  width: '100%',
                  maxWidth: '100%',
                  bgcolor: 'background.default',
                  marginLeft: sidebarOpen ? 0 : '-240px',
                  transition: 'margin-left 0.3s ease',
                  overflow: 'auto',
                }}
              >
                <Toolbar /> {/* Spacer for fixed navbar */}
                <Routes>
                  <Route path="/" element={<Dashboard />} />
                  <Route path="/config" element={<Configuration />} />
                  <Route path="/commands" element={<Commands />} />
                  <Route path="/fota" element={<FOTA />} />
                  <Route path="/logs" element={<Logs />} />
                  {/* <Route path="/utilities" element={<Utilities />} /> */} {/* Hidden - will add later if needed */}
                  <Route path="/testing" element={<Testing />} />
                </Routes>
              </Box>
            </Box>
            <Footer />
          </Box>
        </BrowserRouter>
      </ThemeProvider>
    </QueryClientProvider>
  );
}

export default App;

