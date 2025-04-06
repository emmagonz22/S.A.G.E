import { useBottomTabBarHeight } from '@react-navigation/bottom-tabs';
import { BlurView } from 'expo-blur';
import { StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { View, useTheme } from 'tamagui';

export default function BlurTabBarBackground() {
  
  const theme = useTheme();

  return (
    <View style={StyleSheet.absoluteFill}>
      <BlurView tint="light" style={StyleSheet.absoluteFill} />
    </View>
  );
}

export function useBottomTabOverflow() {
  const tabHeight = useBottomTabBarHeight();
  const { bottom } = useSafeAreaInsets();
  return tabHeight - bottom;
}
