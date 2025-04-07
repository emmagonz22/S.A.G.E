import { useBottomTabBarHeight } from '@react-navigation/bottom-tabs';
import { BlurView } from 'expo-blur';
import { StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { View, useTheme } from 'tamagui';

export default function BlurTabBarBackground() {
  
  const theme = useTheme();
  const { bottom } = useSafeAreaInsets();
  return (

    <View style={[
        {
          position: 'absolute',
          left: 10,
          right: 10,
          bottom: bottom - 10,
          height: 70,
          borderRadius: 16,
          backgroundColor: 'transparent',
          overflow: 'hidden', // Needed for borderRadius to apply to BlurView
        },
      ]}>
      <BlurView tint="light" intensity={100} style={StyleSheet.absoluteFill} />
      <View
        backgroundColor="$background"
        opacity={0.3} // Adjust overlay intensity
        style={StyleSheet.absoluteFill}
      />
    </View>
  );
}

export function useBottomTabOverflow() {
  const tabHeight = useBottomTabBarHeight();
  const { bottom } = useSafeAreaInsets();
  return tabHeight - bottom;
}
