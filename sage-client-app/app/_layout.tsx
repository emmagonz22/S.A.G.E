
// Tamagui imports
import { TamaguiProvider } from '@tamagui/core';
import { config } from '../tamagui.config'

// Expo imports
import { useFonts } from 'expo-font';
import { Stack } from 'expo-router';
import { StatusBar } from 'expo-status-bar';

// React imports
import { useEffect, useState } from 'react';
import 'react-native-reanimated';

// Import our custom BootSplash implementation
import BootSplash from '../bootsplash';
import { ThemeProvider, useTheme as isDarkProvider } from '../context/ThemeProvider';
import { YStack } from 'tamagui';

export default function RootLayout() {
  const [loaded] = useFonts({
    SpaceMono: require('../assets/fonts/SpaceMono-Regular.ttf'),
  });
  const [splashVisible, setSplashVisible] = useState(true);
  // Handle font loading and splash screen
  useEffect(() => {
    if (loaded) {
      // Fonts are loaded, we can hide the splash in a moment
      // We keep splash visible for a short moment to ensure smooth transition
    }
  }, [loaded]);

  if (!loaded) {
    return null;
  }

  return (
    <TamaguiProvider config={config} defaultTheme='light'>
      <ThemeProvider>
        <StatusBarManager />
        <YStack flex={1} backgroundColor="$background">
          <Stack
            screenOptions={{
              // contentStyle sets the background behind your screen component
              contentStyle: {
                backgroundColor: '$background',
              },
              // headerStyle sets the background of the native header bar
            }}
          >
            <Stack.Screen name="(tabs)" options={{ headerShown: false }}  />
            <Stack.Screen name="+not-found" />
          </Stack>
    
          {splashVisible && (
            <BootSplash.BootSplashScreen
              onAnimationComplete={() => setSplashVisible(false)}
            />
          )}
             
        </YStack>
  
      </ThemeProvider>
      
    </TamaguiProvider>
  );
}

function StatusBarManager() {
  const { isDarkMode } = isDarkProvider();
  
  // This will apply the status bar style app-wide
  return <StatusBar style={isDarkMode ? "light" : "dark"} animated={true} />;
}